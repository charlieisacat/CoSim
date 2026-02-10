#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_42_params.I, conv_42_params.J, conv_42_params.K,
  conv_dw_41_out, conv_42_w, conv_42_b, conv_42_out,
  NO_ACTIVATION, conv_42_params.output_scale, true,
  tiled_matmul_type, check, "conv_42");
  return 0;
}
