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

#include "imagenet/mobilenet_params.h"
#include "imagenet/images.h"

// Shared defaults matching imagenet/mobilenet.c
static enum tiled_matmul_type_t tiled_matmul_type = WS;
static bool check = false;

static inline uint64_t read_instret(void) {
  uint64_t instret;
  asm volatile("rdinstret %0" : "=r"(instret));
  return instret;
}


#endif // GEMMINI_MOBILENET_LAYER_COMMON_H
