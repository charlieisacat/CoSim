
#include "mobilenet_layer_common.h"

int main(void) {
static elem_t average[1280][4] row_align(1);

tiled_matmul_nn_auto(fc_53_params.I, fc_53_params.J, fc_53_params.K,
        fc_53_w, average, fc_53_b, fc_53_out,
        NO_ACTIVATION, fc_53_params.output_scale, false,
        tiled_matmul_type, check, "fc_53");

}