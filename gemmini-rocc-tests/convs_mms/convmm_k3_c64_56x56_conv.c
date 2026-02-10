#include "conv_mm_header.h"

int main(void) {

  #define  batch  1
  #define  in_row  56
  #define  in_col  56
  #define  in_ch  64
  #define  out_ch  64
  #define  stride  1
  #define  padding  1
  #define  kernel  3

  // static elem_t input[1 * 56 * 56 * 64];
  // static elem_t weights[64 * 64 * 3 * 3];
  // static acc_t bias[64];
  // static elem_t output[1 * 56 * 56 * 64];

    static elem_t input[in_row * in_col][in_ch];
  static elem_t weights[kernel * kernel * in_ch][out_ch];
  static elem_t output[in_row * in_col][out_ch];
 
  tiled_conv_auto(
    batch, in_row, in_col,
    in_ch,
    out_ch, in_row, in_col,
    stride, 1, 1, padding, kernel,
    false, false, false, false, false,

    (elem_t*)input, (elem_t*)weights, NULL, (elem_t*)output,

    RELU, 0,
    0, 0, 0,

    tiled_matmul_type);


  return 0;
}
