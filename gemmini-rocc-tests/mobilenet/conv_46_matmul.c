#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_46_params.I, conv_46_params.J, conv_46_params.K,
  conv_45_out, conv_46_w, conv_46_b, conv_46_out,
  RELU, conv_46_params.output_scale, true,
  tiled_matmul_type, check, "conv_46");
  return 0;
}
