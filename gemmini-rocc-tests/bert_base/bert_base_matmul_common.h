#pragma once

#include <stdint.h>
#include <stdbool.h>
#ifndef BAREMETAL
#include <sys/mman.h>
#include <stdlib.h>
#endif

#include "include/gemmini.h"
#include "include/gemmini_nn.h"
// Default knobs (same style as mobilenet common)
static enum tiled_matmul_type_t tiled_matmul_type = WS;
static bool check = false;
