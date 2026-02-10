#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_27_params.I, conv_27_params.J, conv_27_params.K,
  conv_dw_26_out, conv_27_w, conv_27_b, conv_27_out,
  NO_ACTIVATION, conv_27_params.output_scale, true,
  tiled_matmul_type, check, "conv_27");
  return 0;
}
