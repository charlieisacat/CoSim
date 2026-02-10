#include "transformer_small_matmul_common.h"

// transformer-small QKV projection i=1
// dim: M=128, N=512, K=512
// strides: A=512, B=512, D=0, C=512

int main(void) {

  static elem_t A[128][512];
  static elem_t B[512][512];
  static acc_t D[512];
  static elem_t C[128][512];

  tiled_matmul_auto(128, 512, 512,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C,
    512, 512, 0, 512,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0,
    false,
    false, false,
    false, false,
    0,
    tiled_matmul_type);

  return 0;
}
