#ifndef GEMMINI_MOBILENET_LAYER_COMMON_H
#define GEMMINI_MOBILENET_LAYER_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef BAREMETAL
#include <sys/mman.h>
#endif

#include "include/gemmini.h"
#include "include/gemmini_nn.h"
#include "include/gemmini_testutils.h"

// Shared defaults matching imagenet/mobilenet.c
static enum tiled_matmul_type_t tiled_matmul_type = WS;
static bool check = false;

#endif // GEMMINI_MOBILENET_LAYER_COMMON_H
