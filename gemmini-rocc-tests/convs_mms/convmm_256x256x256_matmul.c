#include "conv_mm_header.h"

int main(void) {

  static enum tiled_matmul_type_t tiled_matmul_type = WS;

  const int I = 256;
  const int J = 256;
  const int K = 256;

  static elem_t A[256*256];
  static elem_t B[256*256];
  static elem_t C[256*256];

  tiled_matmul_nn_auto(I, J, K,
    (elem_t*)A, (elem_t*)B,
    NULL, (elem_t*)C,
    NO_ACTIVATION, 0, true,
    tiled_matmul_type, check, "matmul_256x256x256");


  return 0;
}
