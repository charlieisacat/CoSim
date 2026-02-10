//===- GemminiConvPass.h - Replace linalg.conv with Gemmini call ---------===//
//
// Replaces bufferized `linalg.conv_2d_nchw_fchw` (memref form) with a call to a
// Gemmini runtime wrapper.
//
// The runtime wrapper is expected to provide:
//   extern "C" void tiled_conv_2d_nchw_fchw_auto_mlir(
//       StridedMemRefType<elem_t, 4>* input,
//       StridedMemRefType<elem_t, 4>* weights,
//       StridedMemRefType<elem_t, 4>* output,
//       int64_t stride_h, int64_t stride_w,
//       int64_t dilation_h, int64_t dilation_w);
//
//===----------------------------------------------------------------------===//

#pragma once

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace mlir {

class GemminiConvPass
    : public PassWrapper<GemminiConvPass, OperationPass<ModuleOp>> {
public:
  GemminiConvPass() = default;

  void getDependentDialects(DialectRegistry &registry) const override;
  void runOnOperation() override;

  StringRef getArgument() const final { return "gemmini-conv"; }
  StringRef getDescription() const final {
    return "Replace bufferized linalg.conv_2d_nchw_fchw with Gemmini tiled_conv_auto";
  }
};

std::unique_ptr<Pass> createGemminiConvPass();

} // namespace mlir
