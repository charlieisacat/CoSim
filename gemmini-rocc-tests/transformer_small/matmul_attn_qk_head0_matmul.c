#include "transformer_small_matmul_common.h"

// transformer-small attention score head=0: attn = Q * K
// dim: M=128, N=128, K=128
// strides: A=512, B=512, D=0, C=128

int main(void) {

  static elem_t A[128][512];
  static elem_t B[128][512];
  static elem_t C[128][128];

  tiled_matmul_auto(128, 128, 128,
    (elem_t*)A, (elem_t*)B,
    NULL, (elem_t*)C,
    512, 128, 0, 128,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0,
    false,
    false, false,
    false, false,
    0,
    tiled_matmul_type);

  return 0;
}
