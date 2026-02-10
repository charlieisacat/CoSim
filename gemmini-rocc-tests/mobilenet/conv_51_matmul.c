#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_51_params.I, conv_51_params.J, conv_51_params.K,
  conv_dw_50_out, conv_51_w, conv_51_b, conv_51_out,
  NO_ACTIVATION, conv_51_params.output_scale, true,
  tiled_matmul_type, check, "conv_51");
  return 0;
}
