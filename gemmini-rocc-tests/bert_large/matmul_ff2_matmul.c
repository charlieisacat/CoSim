#include "bert_large_matmul_common.h"

// bert-large FFN2
// dim: M=128, N=1024, K=4096
// strides: A=4096, B=1024, D=4096, C=4096

int main(void) {

  static elem_t A[128][4096];
  static elem_t B[4096][1024];
  static acc_t D[1024];
  static acc_t C[128][4096];

  tiled_matmul_auto(128, 1024, 4096,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C,
    4096, 4096, 4096, 4096,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, 0,
    true,
    false, false,
    true, false,
    0,
    tiled_matmul_type);

  return 0;
}
