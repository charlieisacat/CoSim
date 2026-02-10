#include "transformer_small_matmul_common.h"

// transformer-small FFN1: out = input * ff1_w (+bias)
// dim: M=128, N=1024, K=512
// strides: A=512, B=1024, D=1024, C=1024

int main(void) {

  static elem_t A[128][512];
  static elem_t B[512][1024];
  static acc_t D[1024];
  static elem_t C[128][1024];

  tiled_matmul_auto(128, 1024, 512,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C,
    512, 512, 1024, 1024,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, ACC_SCALE_IDENTITY,
    true,
    false, false,
    false, false,
    0,
    tiled_matmul_type);

  return 0;
}
