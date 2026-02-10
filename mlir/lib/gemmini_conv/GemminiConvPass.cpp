//===- GemminiConvPass.cpp - Replace linalg.conv with Gemmini call --------===//

#include "GemminiConvPass.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

namespace {

static constexpr StringLiteral kCalleeName = "tiled_conv_2d_nchw_fchw_auto_mlir";

static bool extract2xI64(Operation *op, StringRef attrName, int64_t &v0,
                         int64_t &v1) {
  Attribute a = op->getAttr(attrName);
  if (!a)
    return false;

  if (auto arr = dyn_cast<DenseI64ArrayAttr>(a)) {
    if (arr.size() != 2)
      return false;
    v0 = arr[0];
    v1 = arr[1];
    return true;
  }

  if (auto dense = dyn_cast<DenseElementsAttr>(a)) {
    Type t = dense.getType();
    if (!isa<VectorType>(t))
      return false;
    if (dense.getNumElements() != 2)
      return false;
    auto values = dense.getValues<int64_t>();
    auto iter = values.begin();
    v0 = *iter;
    ++iter;
    v1 = *iter;
    return true;
  }

  return false;
}

static func::FuncOp getOrInsertCallee(ModuleOp module, Type elementType) {
  MLIRContext *ctx = module.getContext();
  if (auto existing = module.lookupSymbol<func::FuncOp>(kCalleeName))
    return existing;

  OpBuilder b(ctx);
  b.setInsertionPointToStart(module.getBody());

  auto dyn4D = MemRefType::get({ShapedType::kDynamic, ShapedType::kDynamic,
                                ShapedType::kDynamic, ShapedType::kDynamic},
                               elementType);
  auto i64 = IntegerType::get(ctx, 64);
  auto fnTy = b.getFunctionType({dyn4D, dyn4D, dyn4D, i64, i64, i64, i64}, {});

  auto fn = b.create<func::FuncOp>(module.getLoc(), kCalleeName, fnTy);
  fn.setPrivate();
  fn->setAttr("llvm.emit_c_interface", b.getUnitAttr());
  return fn;
}

static FailureOr<Value> castToDyn4D(MemRefType targetTy, Value v, OpBuilder &b,
                                   Location loc) {
  auto srcTy = dyn_cast<MemRefType>(v.getType());
  if (!srcTy)
    return failure();
  if (srcTy == targetTy)
    return v;
  if (srcTy.getRank() != 4 || targetTy.getRank() != 4)
    return failure();
  if (srcTy.getElementType() != targetTy.getElementType())
    return failure();
  return b.create<memref::CastOp>(loc, targetTy, v).getResult();
}

} // namespace

void GemminiConvPass::getDependentDialects(DialectRegistry &registry) const {
  registry.insert<arith::ArithDialect, linalg::LinalgDialect, func::FuncDialect,
                  memref::MemRefDialect>();
}

void GemminiConvPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *ctx = module.getContext();

  SmallVector<Operation *, 16> toErase;

  module.walk([&](Operation *op) {
    auto conv = dyn_cast<linalg::Conv2DNchwFchwOp>(op);
    if (!conv)
      return;

    // Expect bufferized form (memref) with no SSA results.
    if (op->getNumResults() != 0) {
      op->emitWarning()
          << "GemminiConvPass expects bufferized linalg.conv_2d_nchw_fchw (memref form). "
          << "Found tensor form with results; skipping.";
      return;
    }

    if (conv.getInputs().size() != 2 || conv.getOutputs().size() != 1) {
      op->emitWarning() << "Unexpected conv operand counts; skipping.";
      return;
    }

    Value input = conv.getInputs()[0];
    Value weights = conv.getInputs()[1];
    Value output = conv.getOutputs()[0];

    auto inTy = dyn_cast<MemRefType>(input.getType());
    auto wTy = dyn_cast<MemRefType>(weights.getType());
    auto outTy = dyn_cast<MemRefType>(output.getType());
    if (!inTy || !wTy || !outTy) {
      op->emitWarning() << "Expected memref operands; skipping.";
      return;
    }

    if (inTy.getRank() != 4 || wTy.getRank() != 4 || outTy.getRank() != 4) {
      op->emitWarning() << "Only rank-4 conv supported; skipping.";
      return;
    }

    Type elemTy = inTy.getElementType();
    if (wTy.getElementType() != elemTy || outTy.getElementType() != elemTy) {
      op->emitWarning() << "Mismatched element types; skipping.";
      return;
    }

    // Gemmini default elem_t is int8_t. For now, require i8 or f32.
    if (!elemTy.isInteger(8) && !elemTy.isF32()) {
      op->emitWarning() << "GemminiConvPass currently requires i8 or f32 element type; skipping.";
      return;
    }

    int64_t strideH = 1, strideW = 1;
    int64_t dilH = 1, dilW = 1;
    (void)extract2xI64(op, "strides", strideH, strideW);
    (void)extract2xI64(op, "dilations", dilH, dilW);

    OpBuilder bld(ctx);
    bld.setInsertionPoint(op);

    func::FuncOp callee = getOrInsertCallee(module, elemTy);
    auto dyn4D = MemRefType::get({ShapedType::kDynamic, ShapedType::kDynamic,
                                  ShapedType::kDynamic, ShapedType::kDynamic},
                                 elemTy);

    auto castIn = castToDyn4D(dyn4D, input, bld, op->getLoc());
    auto castW = castToDyn4D(dyn4D, weights, bld, op->getLoc());
    auto castOut = castToDyn4D(dyn4D, output, bld, op->getLoc());

    if (failed(castIn) || failed(castW) || failed(castOut)) {
      op->emitWarning() << "Failed to cast operands to memref<?x?x?x?xi8>; skipping.";
      return;
    }

    Value cStrideH = bld.create<arith::ConstantIntOp>(op->getLoc(), strideH, /*width=*/64);
    Value cStrideW = bld.create<arith::ConstantIntOp>(op->getLoc(), strideW, /*width=*/64);
    Value cDilH = bld.create<arith::ConstantIntOp>(op->getLoc(), dilH, /*width=*/64);
    Value cDilW = bld.create<arith::ConstantIntOp>(op->getLoc(), dilW, /*width=*/64);

    bld.create<func::CallOp>(op->getLoc(), callee,
                             ValueRange{*castIn, *castW, *castOut, cStrideH, cStrideW, cDilH, cDilW});

    toErase.push_back(op);
  });

  for (Operation *op : toErase)
    op->erase();
}

std::unique_ptr<Pass> mlir::createGemminiConvPass() {
  return std::make_unique<GemminiConvPass>();
}
