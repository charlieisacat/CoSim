#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_53_params.I, conv_53_params.J, conv_53_params.K,
        conv_52_out, conv_53_w, conv_53_b, conv_53_out,
        NO_ACTIVATION, conv_53_params.output_scale, true,
        tiled_matmul_type, check, "conv_53");

    return 0;
}
