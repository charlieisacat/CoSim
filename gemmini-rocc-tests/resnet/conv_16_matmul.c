#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_16_params.I, conv_16_params.J, conv_16_params.K,
        conv_14_out, conv_16_w, conv_16_b, conv_16_out,
        RELU, conv_16_params.output_scale, true,
        tiled_matmul_type, check, "conv_16");

    return 0;
}
