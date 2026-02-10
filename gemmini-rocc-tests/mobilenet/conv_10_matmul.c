#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_10_params.I, conv_10_params.J, conv_10_params.K,
      conv_9_out, conv_10_w, conv_10_b, conv_10_out,
      RELU, conv_10_params.output_scale, true,
    tiled_matmul_type, check, "conv_10");
  return 0;
}
