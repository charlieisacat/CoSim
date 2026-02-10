#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_7_params.I, conv_7_params.J, conv_7_params.K,
      conv_6_out, conv_7_w, conv_7_b, conv_7_out,
      RELU, conv_7_params.output_scale, true,
    tiled_matmul_type, check, "conv_7");
  return 0;
}
