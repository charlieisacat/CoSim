#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_4_params.I, conv_4_params.J, conv_4_params.K,
  conv_3_out, conv_4_w, conv_4_b, conv_4_out,
  RELU, conv_4_params.output_scale, true,
  tiled_matmul_type, check, "conv_4");
  return 0;
}
