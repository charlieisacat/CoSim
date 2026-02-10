#include "conv_mm_header.h"

int main(void) {
  static enum tiled_matmul_type_t tiled_matmul_type = WS;

  const int I = 1024;
  const int J = 1024;
  const int K = 1024;

  static elem_t A[1024*1024];
  static elem_t B[1024*1024];
  static elem_t C[1024*1024];

  tiled_matmul_nn_auto(I, J, K,
    (elem_t*)A, (elem_t*)B,
    NULL, (elem_t*)C,
    NO_ACTIVATION, 0, true,
    tiled_matmul_type, check, "matmul_1024x1024x1024");
  return 0;
}
