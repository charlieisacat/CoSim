INST_LIB_PATH=$BASE_PATH/llvm

llvm-link resnet.ll $BASE_PATH/test_dnn/test.ll $BASE_PATH/gemmini_interface/gemmini_runtime.ll  -o combine.bc
opt -load-pass-plugin $INST_LIB_PATH/build/librename.so -passes=renameBBs < combine.bc > combine_rn.bc
opt -passes=lower-invoke,simplifycfg combine_rn.bc -o opt.bc

# BBV collection, please change to your MLIR path
#opt -load-pass-plugin $INST_LIB_PATH/build/libprofile.so -passes=profile -threshold 2000000 < opt.bc > combine_run_profile.bc
#clang++ combine_run_profile.bc -L/staff/haoxiaoyu/mlir_project/llvm-project/build/externals/llvm-project/install/lib -lmlir_c_runner_utils -lm -L$BASE_PATH/manual/build -lmanual_interface  -L$INST_LIB_PATH/build -lprofile_helper

# Trace collection with simpoit
#opt -load-pass-plugin $INST_LIB_PATH/build/libinstrument.so -passes=instrumentPass   -threshold 2000000 -interval 9 -simpoint 1 < opt.bc > combine_inst.bc

opt -load-pass-plugin $INST_LIB_PATH/build/libinstrument.so -passes=instrumentPass < opt.bc > combine_inst.bc


clang++ combine_inst.bc -L/staff/haoxiaoyu/mlir_project/llvm-project/build/externals/llvm-project/install/lib -lmlir_c_runner_utils -lm -L$BASE_PATH/manual/build -lmanual_interface  -L$INST_LIB_PATH/build -llog_helper -o resnet
#
