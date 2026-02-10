#include "bert_large_matmul_common.h"

// bert-large FFN1
// dim: M=128, N=4096, K=1024
// strides: A=1024, B=4096, D=4096, C=4096

int main(void) {
  static elem_t A[128][1024];
  static elem_t B[1024][4096];
  static acc_t D[4096];
  static elem_t C[128][4096];

  tiled_matmul_auto(128, 4096, 1024,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C,
    1024, 1024, 4096, 4096,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, ACC_SCALE_IDENTITY,
    true,
    false, false,
    false, false,
    0,
    tiled_matmul_type);
  return 0;
}
