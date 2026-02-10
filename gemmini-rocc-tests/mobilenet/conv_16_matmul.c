#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_16_params.I, conv_16_params.J, conv_16_params.K,
  conv_15_out, conv_16_w, conv_16_b, conv_16_out,
  RELU, conv_16_params.output_scale, true,
  tiled_matmul_type, check, "conv_16");
  return 0;
}
