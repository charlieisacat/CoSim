#include "mobilenet_layer_common.h"

int main(void) {

  tiled_conv_dw_auto(
  conv_dw_50_params.batch_size, conv_dw_50_params.in_row_dim, conv_dw_50_params.in_col_dim,
  conv_dw_50_params.in_channels,
  conv_dw_50_params.out_row_dim, conv_dw_50_params.out_col_dim,
  conv_dw_50_params.stride, conv_dw_50_params.padding, conv_dw_50_params.kernel_size,
  
  (elem_t*)conv_49_out, (elem_t*)conv_dw_50_w, NULL,  (elem_t*)conv_dw_50_out,
  
  RELU, conv_dw_50_params.output_scale,
  conv_dw_50_params.pool_size, 0, conv_dw_50_params.pool_padding,
  
  tiled_matmul_type);
  return 0;
}
