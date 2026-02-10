#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_13_params.I, conv_13_params.J, conv_13_params.K,
  conv_12_out, conv_13_w, conv_13_b, conv_13_out,
  RELU, conv_13_params.output_scale, true,
  tiled_matmul_type, check, "conv_13");
  return 0;
}
