#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_37_params.I, conv_37_params.J, conv_37_params.K,
  conv_36_out, conv_37_w, conv_37_b, conv_37_out,
  RELU, conv_37_params.output_scale, true,
  tiled_matmul_type, check, "conv_37");
  return 0;
}
