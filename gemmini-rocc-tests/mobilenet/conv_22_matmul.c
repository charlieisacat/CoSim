#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_22_params.I, conv_22_params.J, conv_22_params.K,
  conv_21_out, conv_22_w, conv_22_b, conv_22_out,
  RELU, conv_22_params.output_scale, true,
  tiled_matmul_type, check, "conv_22");
  return 0;
}
