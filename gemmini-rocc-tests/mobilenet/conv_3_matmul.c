#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_3_params.I, conv_3_params.J, conv_3_params.K,
  conv_dw_2_out, conv_3_w, conv_3_b, conv_3_out,
  NO_ACTIVATION, conv_3_params.output_scale, true,
  tiled_matmul_type, check, "conv_3");
  return 0;
}
