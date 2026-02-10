#include "conv_mm_header.h"

int main(void) {
  static enum tiled_matmul_type_t tiled_matmul_type = WS;

  const int I = 2048;
  const int J = 2048;
  const int K = 2048;

  static elem_t A[2048*2048];
  static elem_t B[2048*2048];
  static elem_t C[2048*2048];
  tiled_matmul_nn_auto(I, J, K,
    (elem_t*)A, (elem_t*)B,
    NULL, (elem_t*)C,
    NO_ACTIVATION, 0, true,
    tiled_matmul_type, check, "matmul_2048x2048x2048");


  return 0;
}
