#include "bert_base_matmul_common.h"

// bert-base output projection: out_buf_acc = out_buf * Wo
// dim: (seq_len=128) x (hidden_dim=768) times K=hidden_dim_compressed=768
// strides: A=768, B=768, D=0, C=768

int main(void) {
  static elem_t A[128][768];
  static elem_t B[768][768];
  static acc_t  D[768];
  static acc_t  C_acc[128][768];

  tiled_matmul_auto(128, 768, 768,
    (elem_t*)A, (elem_t*)B,
    (acc_t*)D, (elem_t*)C_acc,
    /*stride_A=*/768, /*stride_B=*/768, /*stride_D=*/0, /*stride_C=*/768,
    MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY,
    NO_ACTIVATION, ACC_SCALE_IDENTITY, /*bert_scale=*/0,
    /*repeating_bias=*/false,
    /*a_transpose=*/false, /*transpose_B=*/false,
    /*full_C=*/true, /*low_D=*/false,
    0,
    tiled_matmul_type);
  return 0;
}
