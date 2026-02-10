#include "mobilenet_layer_common.h"

int main(void) {

  tiled_matmul_nn_auto(conv_9_params.I, conv_9_params.J, conv_9_params.K,
      conv_dw_8_out, conv_9_w, conv_9_b, conv_9_out,
      NO_ACTIVATION, conv_9_params.output_scale, true,
    tiled_matmul_type, check, "conv_9");
  return 0;
}
