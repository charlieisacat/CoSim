#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_18_params.I, conv_18_params.J, conv_18_params.K,
  conv_dw_17_out, conv_18_w, conv_18_b, conv_18_out,
  NO_ACTIVATION, conv_18_params.output_scale, true,
  tiled_matmul_type, check, "conv_18");
  return 0;
}
