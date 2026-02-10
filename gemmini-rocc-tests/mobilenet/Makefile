# Build ResNet layers tests on a regular x86/Linux host with GCC/LLVM

CC = clang
INST_LIB_PATH=$(BASE_PATH)/llvm
MANUAL=$(BASE_PATH)/manual

# Find all layer source files we want to build runners for
SRCS := $(wildcard conv_*.c) $(wildcard fc_*.c)
# Define targets (executables) for each source file
BINS := $(SRCS:.c=)

# Common headers
FILES_COMMON = mobilenet_layer_common.h ../include/gemmini.h $(MANUAL)/interface.h ../include/gemmini_params.h

# Include paths
CPPFLAGS ?= -I.. -I$(MANUAL)

# CFLAGS adapted from imagenet/Makefile
# Note: -emit-llvm is used to create bitcode (.bc) objects
CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-incompatible-pointer-types -emit-llvm  -ffp-contract=off -c -fno-builtin -fno-vectorize -fno-slp-vectorize

LDFLAGS ?=
LDLIBS  ?= -lm

.PHONY: all clean

all: $(BINS)

# Pattern rule to build each executable from its .c file
# 1. Compile C to LLVM Bitcode (.bc)
# 2. Link bitcode (self-link to rename/prepare)
# 3. Apply renameBBs pass
# 4. Link with libraries to create clean executable
# 5. Apply instrumentPass
# 6. Link with libraries to create instrumented executable
%: %.c $(FILES_COMMON)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@.bc
	llvm-link $@.bc -o $@_run.bc
	opt -load-pass-plugin $(INST_LIB_PATH)/build/librename.so -passes=renameBBs < $@_run.bc > $@_run_rn.bc
	clang++ $(LDFLAGS) -lm $@_run_rn.bc -L $(INST_LIB_PATH)/build -llog_helper -L$(MANUAL)/build -lmanual_interface -o $@_clean
	opt -load-pass-plugin $(INST_LIB_PATH)/build/libinstrument.so -passes=instrumentPass < $@_run_rn.bc > $@_run_inst.bc
	clang++ $(LDFLAGS) -lm $@_run_inst.bc -L $(INST_LIB_PATH)/build -llog_helper -L$(MANUAL)/build -lmanual_interface -o $@

clean:
	rm -f $(BINS) *.bc *_clean *_run *_run_rn *_run_inst *.o *.txt
