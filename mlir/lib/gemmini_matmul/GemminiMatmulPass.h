//===- GemminiMatmulPass.h - Replace linalg.matmul with Gemmini call -----===//
//
// A simple MLIR pass that replaces bufferized `linalg.matmul` (memref form)
// with a call to a Gemmini runtime wrapper function.
//
// The runtime wrapper is expected to provide:
//   extern "C" void tiled_matmul_nn_auto_mlir(
//       StridedMemRefType<elem_t, 2>* A,
//       StridedMemRefType<elem_t, 2>* B,
//       StridedMemRefType<elem_t, 2>* C);
//
// The pass will cast operands to `memref<?x?xi8>` and emit:
//   func.call @tiled_matmul_nn_auto_mlir(%A, %B, %C)
//
//===----------------------------------------------------------------------===//

#pragma once

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace mlir {

class GemminiMatmulPass
    : public PassWrapper<GemminiMatmulPass, OperationPass<ModuleOp>> {
public:
  GemminiMatmulPass() = default;

  void getDependentDialects(DialectRegistry &registry) const override;
  void runOnOperation() override;

  StringRef getArgument() const final { return "gemmini-matmul"; }
  StringRef getDescription() const final {
    return "Replace bufferized linalg.matmul with Gemmini tiled_matmul_nn_auto";
  }
};

std::unique_ptr<Pass> createGemminiMatmulPass();

} // namespace mlir
