#include "mobilenet_layer_common.h"

int main(void) {

  tiled_conv_dw_auto(
  conv_dw_26_params.batch_size, conv_dw_26_params.in_row_dim, conv_dw_26_params.in_col_dim,
  conv_dw_26_params.in_channels,
  conv_dw_26_params.out_row_dim, conv_dw_26_params.out_col_dim,
  conv_dw_26_params.stride, conv_dw_26_params.padding, conv_dw_26_params.kernel_size,
  
  (elem_t*)conv_25_out, (elem_t*)conv_dw_26_w, NULL,  (elem_t*)conv_dw_26_out,
  
  RELU, conv_dw_26_params.output_scale,
  conv_dw_26_params.pool_size, 0, conv_dw_26_params.pool_padding,
  
  tiled_matmul_type);
  return 0;
}
