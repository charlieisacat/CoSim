#include "conv_mm_header.h"

int main(void) {
  static enum tiled_matmul_type_t tiled_matmul_type = WS;

  const int I = 512;
  const int J = 512;
  const int K = 512;

  static elem_t A[512*512];
  static elem_t B[512*512];
  static elem_t C[512*512];


  tiled_matmul_nn_auto(I, J, K,
    (elem_t*)A, (elem_t*)B,
    NULL, (elem_t*)C,
    NO_ACTIVATION, 0, true,
    tiled_matmul_type, check, "matmul_512x512x512");



  return 0;
}
