#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_12_params.I, conv_12_params.J, conv_12_params.K,
  conv_dw_11_out, conv_12_w, conv_12_b, conv_12_out,
  NO_ACTIVATION, conv_12_params.output_scale, true,
  tiled_matmul_type, check, "conv_12");
  return 0;
}
