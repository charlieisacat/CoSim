#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_34_params.I, conv_34_params.J, conv_34_params.K,
  conv_33_out, conv_34_w, conv_34_b, conv_34_out,
  RELU, conv_34_params.output_scale, true,
  tiled_matmul_type, check, "conv_34");
  return 0;
}
