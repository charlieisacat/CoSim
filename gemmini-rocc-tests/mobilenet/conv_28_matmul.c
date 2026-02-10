#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_28_params.I, conv_28_params.J, conv_28_params.K,
  conv_27_out, conv_28_w, conv_28_b, conv_28_out,
  RELU, conv_28_params.output_scale, true,
  tiled_matmul_type, check, "conv_28");
  return 0;
}
