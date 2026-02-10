#include "bert_base_matmul_common.h"

// bert-base attention score (head=3): attn = Q * K
// dim: M=128, N=128, K=64
// strides: A=768, B=768, D=0, C=128

int main(void) {
  static elem_t A[128][768];
  static elem_t B[128][768];
  static elem_t C[128][128];

  tiled_matmul_auto(128, 128, 64,
    (elem_t*)A, (elem_t*)B,
    /*D=*/NULL, (elem_t*)C,
    /*stride_A=*/768, /*stride_B=*/768, /*stride_D=*/0, /*stride_C=*/128,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, /*bert_scale=*/0,
    /*repeating_bias=*/false,
    /*a_transpose=*/false, /*transpose_B=*/false,
    /*full_C=*/false, /*low_D=*/false,
    0,
    tiled_matmul_type);
  return 0;
}
