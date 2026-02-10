rm *.mlir

iree-import-onnx resnet.onnx --opset-version 17 -o resnet.mlir
torch-mlir-opt --torch-onnx-to-torch-backend-pipeline --torch-backend-to-linalg-on-tensors-backend-pipeline resnet.mlir > linalg.mlir
mlir-opt linalg.mlir --one-shot-bufferize="bufferize-function-boundaries" > bff.mlir

$BASE_PATH/mlir/build/tools/customize bff.mlir -gemmini-matmul -gemmini-conv -o conv.mlir

mlir-opt conv.mlir  --convert-linalg-to-loops --convert-scf-to-cf --convert-cf-to-llvm --expand-strided-metadata --lower-affine --finalize-memref-to-llvm  --convert-arith-to-llvm   --convert-index-to-llvm   --convert-math-to-llvm   --convert-func-to-llvm  --convert-vector-to-llvm   --reconcile-unrealized-casts -o lowered.mlir

mlir-translate lowered.mlir --mlir-to-llvmir -o resnet.ll
