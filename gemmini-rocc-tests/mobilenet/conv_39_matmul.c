#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_39_params.I, conv_39_params.J, conv_39_params.K,
  conv_dw_38_out, conv_39_w, conv_39_b, conv_39_out,
  NO_ACTIVATION, conv_39_params.output_scale, true,
  tiled_matmul_type, check, "conv_39");
  return 0;
}
