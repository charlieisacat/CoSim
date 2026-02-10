#include "bert_large_matmul_common.h"

// bert-large attention score head=12: attn = Q * K
// dim: M=128, N=128, K=64
// strides: A=1024, B=1024, D=0, C=128

int main(void) {

  static elem_t A[128][1024];
  static elem_t B[128][1024];
  static elem_t C[128][128];

  tiled_matmul_auto(128, 128, 64,
    (elem_t*)A, (elem_t*)B,
    NULL, (elem_t*)C,
    1024, 1024, 0, 128,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0,
    false,
    false, false,
    false, false,
    0,
    tiled_matmul_type);

  return 0;
}
