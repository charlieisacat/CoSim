#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_21_params.I, conv_21_params.J, conv_21_params.K,
  conv_dw_20_out, conv_21_w, conv_21_b, conv_21_out,
  NO_ACTIVATION, conv_21_params.output_scale, true,
  tiled_matmul_type, check, "conv_21");
  return 0;
}
