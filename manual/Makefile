KERN=test
ALG=test
INST_LIB_PATH=$(BASE_PATH)/llvm
MANUAL=$(BASE_PATH)/manual

CC = clang
CFLAGS = -emit-llvm -O3 -ffp-contract=off -c -fno-builtin -fno-vectorize -fno-slp-vectorize

SRCS=test.c interface.c
FILES=$(SRCS) interface.h
BC=interface.bc test.bc

$(KERN): $(FILES)
	$(CC) $(CFLAGS) $(SRCS)
	llvm-link $(BC) -o $(KERN)_run.bc
	opt -load-pass-plugin $(INST_LIB_PATH)/build/librename.so -passes=renameBBs < $(KERN)_run.bc > $(KERN)_run_rn.bc
	clang++   -lm $(KERN)_run_rn.bc -L $(INST_LIB_PATH)/build -llog_helper -o $(KERN)_run_clean
	opt -load-pass-plugin $(INST_LIB_PATH)/build/libinstrument.so -passes=instrumentPass < $(KERN)_run_rn.bc > $(KERN)_run_inst.bc
	clang++   -lm $(KERN)_run_inst.bc -L $(INST_LIB_PATH)/build -llog_helper -o $(KERN)_run

clean:
	rm -f $(KERN)_run*.bc *.ll $(KERN)_run example.txt $(KERN)_run_clean *.bc
