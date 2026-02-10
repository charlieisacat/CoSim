#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_30_params.I, conv_30_params.J, conv_30_params.K,
  conv_dw_29_out, conv_30_w, conv_30_b, conv_30_out,
  NO_ACTIVATION, conv_30_params.output_scale, true,
  tiled_matmul_type, check, "conv_30");
  return 0;
}
