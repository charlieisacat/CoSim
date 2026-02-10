#include "mobilenet_layer_common.h"

int main(void) {


  tiled_conv_auto(
  conv_1_params.batch_size, conv_1_params.in_row_dim, conv_1_params.in_col_dim,
  conv_1_params.in_channels,
  conv_1_params.out_channels, conv_1_params.out_row_dim, conv_1_params.out_col_dim,
  conv_1_params.stride, 1, 1, conv_1_params.padding, conv_1_params.kernel_size,
  false, false, false, false, false,
  
  (elem_t*)images, (elem_t*)conv_1_w, NULL, (elem_t*)conv_1_out,
  
  RELU, conv_1_params.output_scale,
  conv_1_params.pool_size, 0, conv_1_params.pool_padding,
  
  tiled_matmul_type);
  return 0;
}
