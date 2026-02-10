#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_48_params.I, conv_48_params.J, conv_48_params.K,
  conv_dw_47_out, conv_48_w, conv_48_b, conv_48_out,
  NO_ACTIVATION, conv_48_params.output_scale, true,
  tiled_matmul_type, check, "conv_48");
  return 0;
}
