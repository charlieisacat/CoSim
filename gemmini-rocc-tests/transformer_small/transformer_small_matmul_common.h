#pragma once

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "include/gemmini.h"
#include "include/gemmini_testutils.h"

// cycle/instret instrumentation intentionally removed

// Keep defaults in one place
static const enum tiled_matmul_type_t tiled_matmul_type = WS;
static const bool check = false;

