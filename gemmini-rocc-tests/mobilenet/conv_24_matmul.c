#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_24_params.I, conv_24_params.J, conv_24_params.K,
  conv_dw_23_out, conv_24_w, conv_24_b, conv_24_out,
  NO_ACTIVATION, conv_24_params.output_scale, true,
  tiled_matmul_type, check, "conv_24");
  return 0;
}
