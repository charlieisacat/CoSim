# Build AlexNet test on a regular x86/Linux host with GCC

CC = clang
# KERN=resnet
KERN=mobilenet
INST_LIB_PATH=$(BASE_PATH)/llvm
MANUAL=$(BASE_PATH)/manual

# TARGET := resnet
# SRCS   := resnet50.c
TARGET := mobilenet
SRCS   := mobilenet.c

# OBJS   := $(SRCS:.c=.o)
# BC=resnet50.bc
BC=mobilenet.bc

FILES=$(SRCS) ../include/gemmini.h $(MANUAL)/interface.h ../include/gemmini_params.h

# The source includes headers as "include/..." so we add repo root to -I.
# interface.h lives in $(MANUAL).
CPPFLAGS ?= -I.. -I$(MANUAL)

CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-incompatible-pointer-types -emit-llvm  -ffp-contract=off -c -fno-builtin -fno-vectorize -fno-slp-vectorize

LDFLAGS ?=
LDLIBS  ?= -lm

# Default runtime args; override: make run RUN_ARGS="cpu"
RUN_ARGS ?= cpu

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(FILES)
	$(CC) $(CFLAGS) $(SRCS)
	llvm-link $(BC) -o ${KERN}_run.bc
	opt -load-pass-plugin $(INST_LIB_PATH)/build/librename.so -passes=renameBBs < $(KERN)_run.bc > $(KERN)_run_rn.bc
	clang++   -lm $(KERN)_run_rn.bc -L $(INST_LIB_PATH)/build -llog_helper -L$(MANUAL)/build -lmanual_interface -o $(KERN)_run_clean
	opt -load-pass-plugin $(INST_LIB_PATH)/build/libinstrument.so -passes=instrumentPass < $(KERN)_run_rn.bc > $(KERN)_run_inst.bc
	clang++   -lm $(KERN)_run_inst.bc -L $(INST_LIB_PATH)/build -llog_helper  -L$(MANUAL)/build -lmanual_interface -o $(KERN)_run

# %.o: %.c
# 	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET) $(RUN_ARGS)

clean:
	rm -f $(TARGET) $(OBJS) *.o *.bc ${KERN}_run ${KERN}_run_clean *.txt *.ll
