#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_49_params.I, conv_49_params.J, conv_49_params.K,
  conv_48_out, conv_49_w, conv_49_b, conv_49_out,
  RELU, conv_49_params.output_scale, true,
  tiled_matmul_type, check, "conv_49");
  return 0;
}
