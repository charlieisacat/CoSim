#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_31_params.I, conv_31_params.J, conv_31_params.K,
  conv_30_out, conv_31_w, conv_31_b, conv_31_out,
  RELU, conv_31_params.output_scale, true,
  tiled_matmul_type, check, "conv_31");
  return 0;
}
