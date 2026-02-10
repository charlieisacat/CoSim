#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_36_params.I, conv_36_params.J, conv_36_params.K,
  conv_dw_35_out, conv_36_w, conv_36_b, conv_36_out,
  NO_ACTIVATION, conv_36_params.output_scale, true,
  tiled_matmul_type, check, "conv_36");
  return 0;
}
