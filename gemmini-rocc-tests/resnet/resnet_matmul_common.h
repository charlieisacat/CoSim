#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BAREMETAL
#include <sys/mman.h>
#endif

#include "include/gemmini.h"
#include "include/gemmini_nn.h"
#include "include/gemmini_testutils.h"

#include "../imagenet/resnet50_params.h"
#include "../imagenet/images.h"


static inline enum tiled_matmul_type_t get_tiled_matmul_type(int argc, char* argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "OS") == 0) return OS;
        if (strcmp(argv[1], "WS") == 0) return WS;
        if (strcmp(argv[1], "CPU") == 0) return CPU;
        else assert(0 && "Unknown tiled_matmul_type_t");
    }
    return OS;
}
