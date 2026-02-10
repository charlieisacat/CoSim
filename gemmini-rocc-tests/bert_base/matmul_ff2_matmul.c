#include "bert_base_matmul_common.h"

// bert-base FFN second projection: out_buf_acc = out_buf * ff2_w (+bias)
// dim: (seq_len=128) x (hidden_dim=768) times K=expansion_dim=3072
// strides: A=3072, B=768, D=3072, C=3072

int main(void) {
  static elem_t A[128][3072];
  static elem_t B[3072][768];
  static acc_t  D[768];
  static acc_t  C_acc[128][768];

  tiled_matmul_auto(128, 768, 3072,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C_acc,
    /*stride_A=*/3072, /*stride_B=*/3072, /*stride_D=*/768, /*stride_C=*/768,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, /*bert_scale=*/0,
    /*repeating_bias=*/true,
    /*a_transpose=*/false, /*transpose_B=*/false,
    /*full_C=*/true, /*low_D=*/false,
    0,
    tiled_matmul_type);
  return 0;
}
