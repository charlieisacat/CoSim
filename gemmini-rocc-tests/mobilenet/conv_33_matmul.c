#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_33_params.I, conv_33_params.J, conv_33_params.K,
  conv_dw_32_out, conv_33_w, conv_33_b, conv_33_out,
  NO_ACTIVATION, conv_33_params.output_scale, true,
  tiled_matmul_type, check, "conv_33");
  return 0;
}
