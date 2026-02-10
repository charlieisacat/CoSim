#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_6_params.I, conv_6_params.J, conv_6_params.K,
      conv_dw_5_out, conv_6_w, conv_6_b, conv_6_out,
      NO_ACTIVATION, conv_6_params.output_scale, true,
    tiled_matmul_type, check, "conv_6");
  return 0;
}
