#include "bert_base_matmul_common.h"

// bert-base attention value (head=1): out = attn * V
// dim: M=128, N=64, K=128
// strides: A=128, B=768, D=0, C=768

int main(void) {
  static elem_t A[128][128];
  static elem_t B[128][768];
  static elem_t C[128][768];

  tiled_matmul_auto(128, 64, 128,
    (elem_t*)A, (elem_t*)B,
    /*D=*/NULL, (elem_t*)C,
    /*stride_A=*/128, /*stride_B=*/768, /*stride_D=*/0, /*stride_C=*/768,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, /*bert_scale=*/0,
    /*repeating_bias=*/false,
    /*a_transpose=*/false, /*transpose_B=*/false,
    /*full_C=*/false, /*low_D=*/false,
    0,
    tiled_matmul_type);
  return 0;
}
