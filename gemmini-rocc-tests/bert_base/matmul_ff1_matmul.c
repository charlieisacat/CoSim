#include "bert_base_matmul_common.h"

// bert-base FFN first projection: out_buf = input * ff1_w (+bias)
// dim: (seq_len=128) x (expansion_dim=3072) times K=hidden_dim=768
// strides: A=768, B=3072, D=3072, C=3072

int main(void) {
  static elem_t A[128][768];
  static elem_t B[768][3072];
  static acc_t  D[3072];
  static elem_t C[128][3072];

  tiled_matmul_auto(128, 3072, 768,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C,
    /*stride_A=*/768, /*stride_B=*/768, /*stride_D=*/3072, /*stride_C=*/3072,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, /*bert_scale=*/ACC_SCALE_IDENTITY,
    /*repeating_bias=*/true,
    /*a_transpose=*/false, /*transpose_B=*/false,
    /*full_C=*/false, /*low_D=*/false,
    0,
    tiled_matmul_type);
  return 0;
}
