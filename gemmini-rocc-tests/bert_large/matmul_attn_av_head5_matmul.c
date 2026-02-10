#include "bert_large_matmul_common.h"

// bert-large attention value head=5: out = attn * V
// dim: M=128, N=64, K=128
// strides: A=128, B=1024, D=0, C=1024

int main(void) {

  static elem_t A[128][128];
  static elem_t B[64][1024];
  static elem_t C[128][1024];

  tiled_matmul_auto(128, 64, 128,
    (elem_t*)A, (elem_t*)B,
    NULL, (elem_t*)C,
    128, 1024, 0, 1024,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0,
    false,
    false, false,
    false, false,
    0,
    tiled_matmul_type);

  return 0;
}
