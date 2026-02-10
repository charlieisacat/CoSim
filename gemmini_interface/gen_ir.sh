clang -O2 -S -emit-llvm mlir_gemmini_runtime.cpp -I$BASE_PATH/gemmini_interface/include -o gemmini_runtime.ll -I/staff/haoxiaoyu/mlir_project/llvm-project/build/externals/llvm-project/install/include
