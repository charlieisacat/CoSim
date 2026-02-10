#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_45_params.I, conv_45_params.J, conv_45_params.K,
  conv_dw_44_out, conv_45_w, conv_45_b, conv_45_out,
  NO_ACTIVATION, conv_45_params.output_scale, true,
  tiled_matmul_type, check, "conv_45");
  return 0;
}
