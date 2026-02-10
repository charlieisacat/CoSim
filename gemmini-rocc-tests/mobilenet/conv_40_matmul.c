#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_40_params.I, conv_40_params.J, conv_40_params.K,
  conv_39_out, conv_40_w, conv_40_b, conv_40_out,
  RELU, conv_40_params.output_scale, true,
  tiled_matmul_type, check, "conv_40");
  return 0;
}
