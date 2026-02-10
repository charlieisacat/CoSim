#include "bert_large_matmul_common.h"

// bert-large QKV projection i=2
// dim: M=128, N=1024, K=1024
// strides: A=1024, B=1024, D=0, C=1024

int main(void) {

  static elem_t A[128][1024];
  static elem_t B[1024][1024];
  static acc_t D[1024];
  static elem_t C[128][1024];

  tiled_matmul_auto(128, 1024, 1024,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C,
    1024, 1024, 0, 1024,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0,
    false,
    false, false,
    false, false,
    0,
    tiled_matmul_type);

  return 0;
}
