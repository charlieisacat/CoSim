//===- GemminiMatmulPass.cpp - Replace linalg.matmul with Gemmini call ----===//

#include "GemminiMatmulPass.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

static constexpr StringLiteral kCalleeName = "tiled_matmul_nn_auto_mlir";

static func::FuncOp getOrInsertCallee(ModuleOp module, Type elementType) {
  MLIRContext *ctx = module.getContext();
  if (auto existing = module.lookupSymbol<func::FuncOp>(kCalleeName)) {
    auto fnTy = existing.getFunctionType();
    if (fnTy.getNumInputs() == 3 && fnTy.getNumResults() == 0) {
      auto aTy = dyn_cast<MemRefType>(fnTy.getInput(0));
      auto bTy = dyn_cast<MemRefType>(fnTy.getInput(1));
      auto cTy = dyn_cast<MemRefType>(fnTy.getInput(2));
      if (aTy && bTy && cTy && aTy.getRank() == 2 && bTy.getRank() == 2 &&
          cTy.getRank() == 2 && aTy.getElementType() == elementType &&
          bTy.getElementType() == elementType && cTy.getElementType() == elementType) {
        return existing;
      }
    }

    existing->emitWarning()
        << "Existing callee '" << kCalleeName
        << "' has an incompatible signature; GemminiMatmulPass will not rewrite ops of element type "
        << elementType;
    return existing;
  }

  OpBuilder b(ctx);
  b.setInsertionPointToStart(module.getBody());

  // Use fully dynamic 2D memrefs; callers will `memref.cast`.
  auto dyn2D = MemRefType::get({ShapedType::kDynamic, ShapedType::kDynamic}, elementType);
  auto fnTy = b.getFunctionType({dyn2D, dyn2D, dyn2D}, {});

  auto fn = b.create<func::FuncOp>(module.getLoc(), kCalleeName, fnTy);
  fn.setPrivate();

  // Ensure lowering uses the C interface ABI (pointers to StridedMemRefType).
  fn->setAttr("llvm.emit_c_interface", b.getUnitAttr());

  return fn;
}

static FailureOr<Value> castToDyn2D(MemRefType targetTy, Value v, OpBuilder &b,
                                   Location loc) {
  auto srcTy = dyn_cast<MemRefType>(v.getType());
  if (!srcTy)
    return failure();

  if (srcTy == targetTy)
    return v;

  // Only allow casts that keep element type and rank.
  if (srcTy.getRank() != 2 || targetTy.getRank() != 2)
    return failure();
  if (srcTy.getElementType() != targetTy.getElementType())
    return failure();

  // memref.cast supports static<->dynamic shape changes.
  return b.create<memref::CastOp>(loc, targetTy, v).getResult();
}

} // namespace

void GemminiMatmulPass::getDependentDialects(DialectRegistry &registry) const {
  registry.insert<linalg::LinalgDialect, func::FuncDialect, memref::MemRefDialect>();
}

void GemminiMatmulPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *ctx = module.getContext();

  // We rewrite in-place while walking; collect ops to erase after.
  SmallVector<linalg::MatmulOp, 16> toErase;

  module.walk([&](linalg::MatmulOp matmul) {
    Location loc = matmul.getLoc();

    // This pass is intended for the bufferized (memref) form:
    //   linalg.matmul ins(%A, %B : memref<..>, memref<..>) outs(%C : memref<..>)
    // which produces no SSA results.
    if (matmul->getNumResults() != 0) {
      matmul->emitWarning()
          << "GemminiMatmulPass expects bufferized linalg.matmul (memref form). "
          << "Found tensor form with results; skipping.";
      return;
    }

    if (matmul.getInputs().size() != 2 || matmul.getOutputs().size() != 1) {
      matmul->emitWarning() << "Unexpected matmul operand counts; skipping.";
      return;
    }

    Value a = matmul.getInputs()[0];
    Value b = matmul.getInputs()[1];
    Value c = matmul.getOutputs()[0];

    auto aTy = dyn_cast<MemRefType>(a.getType());
    auto bTy = dyn_cast<MemRefType>(b.getType());
    auto cTy = dyn_cast<MemRefType>(c.getType());

    if (!aTy || !bTy || !cTy) {
      matmul->emitWarning() << "Expected memref operands; skipping.";
      return;
    }

    Type elemTy = aTy.getElementType();
    if (bTy.getElementType() != elemTy || cTy.getElementType() != elemTy) {
      matmul->emitWarning() << "Mismatched element types; skipping.";
      return;
    }

    // In practice, Gemmini commonly uses i8 (int8_t). Some setups may build
    // Gemmini with elem_t=float and use f32. Restrict to these two for now.
    bool supportedElem = elemTy.isInteger(8) || elemTy.isF32();
    bool isInt = elemTy.isInteger(8);
    bool isF32 = elemTy.isF32();

    matmul->emitWarning()
          << "isInt " << isInt << " isF32 " << isF32 << "\n";

    if (!supportedElem) {
      matmul->emitWarning()
          << "Unsupported element type for GemminiMatmulPass: " << elemTy
          << " (expected i8 or f32); skipping.";
      return;
    }

    if (aTy.getRank() != 2 || bTy.getRank() != 2 || cTy.getRank() != 2) {
      matmul->emitWarning() << "Only rank-2 matmul supported; skipping.";
      return;
    }

    // Create/lookup the callee with a matching element type.
    func::FuncOp callee = getOrInsertCallee(module, elemTy);
    auto dyn2D = MemRefType::get({ShapedType::kDynamic, ShapedType::kDynamic}, elemTy);

    OpBuilder bld(ctx);
    bld.setInsertionPoint(matmul);

    auto castA = castToDyn2D(dyn2D, a, bld, loc);
    auto castB = castToDyn2D(dyn2D, b, bld, loc);
    auto castC = castToDyn2D(dyn2D, c, bld, loc);

    if (failed(castA) || failed(castB) || failed(castC)) {
      matmul->emitWarning() << "Failed to cast operands to memref<?x?xi8>; skipping.";
      return;
    }

    bld.create<func::CallOp>(loc, callee, ValueRange{*castA, *castB, *castC});

    toErase.push_back(matmul);
  });

  for (linalg::MatmulOp m : toErase)
    m.erase();
}

std::unique_ptr<Pass> mlir::createGemminiMatmulPass() {
  return std::make_unique<GemminiMatmulPass>();
}
