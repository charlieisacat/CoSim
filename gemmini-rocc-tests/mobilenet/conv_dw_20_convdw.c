#include "mobilenet_layer_common.h"

int main(void) {

  tiled_conv_dw_auto(
  conv_dw_20_params.batch_size, conv_dw_20_params.in_row_dim, conv_dw_20_params.in_col_dim,
  conv_dw_20_params.in_channels,
  conv_dw_20_params.out_row_dim, conv_dw_20_params.out_col_dim,
  conv_dw_20_params.stride, conv_dw_20_params.padding, conv_dw_20_params.kernel_size,
  
  (elem_t*)conv_19_out, (elem_t*)conv_dw_20_w, NULL,  (elem_t*)conv_dw_20_out,
  
  RELU, conv_dw_20_params.output_scale,
  conv_dw_20_params.pool_size, 0, conv_dw_20_params.pool_padding,
  
  tiled_matmul_type);
  return 0;
}
