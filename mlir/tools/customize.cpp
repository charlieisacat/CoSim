#include "mlir/include/mlir/InitAllDialects.h"
#include "mlir/include/mlir/Pass/PassManager.h"
#include "mlir/include/mlir/Pass/PassRegistry.h"
#include "mlir/include/mlir/Tools/mlir-opt/MlirOptMain.h"

#include "lib/gemmini_conv/GemminiConvPass.h"
#include "lib/gemmini_matmul/GemminiMatmulPass.h"

int main(int argc, char **argv) {
  mlir::DialectRegistry registry;
  mlir::registerAllDialects(registry);

  mlir::PassRegistration<mlir::GemminiMatmulPass>();
  mlir::PassRegistration<mlir::GemminiConvPass>();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "Customize Pass Driver", registry));
}
