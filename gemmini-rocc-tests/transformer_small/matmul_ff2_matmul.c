#include "transformer_small_matmul_common.h"

// transformer-small FFN2: out = out_buf * ff2_w (+bias)
// dim: M=128, N=512, K=1024
// strides: A=1024, B=512, D=1024, C=1024

int main(void) {

  static elem_t A[128][1024];
  static elem_t B[1024][512];
  static acc_t D[512];
  static acc_t C[128][1024];

  tiled_matmul_auto(128, 512, 1024,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C,
    1024, 1024, 1024, 1024,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0,
    true,
    false, false,
    true, false,
    0,
    tiled_matmul_type);

  return 0;
}
