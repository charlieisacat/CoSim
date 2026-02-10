#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_52_params.I, conv_52_params.J, conv_52_params.K,
  conv_51_out, conv_52_w, conv_52_b, conv_52_out,
  RELU, conv_52_params.output_scale, true,
  tiled_matmul_type, check, "conv_52");
  return 0;
}
