#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_15_params.I, conv_15_params.J, conv_15_params.K,
  conv_dw_14_out, conv_15_w, conv_15_b, conv_15_out,
  NO_ACTIVATION, conv_15_params.output_scale, true,
  tiled_matmul_type, check, "conv_15");
  return 0;
}
