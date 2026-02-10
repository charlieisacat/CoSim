#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_41_params.I, conv_41_params.J, conv_41_params.K,
        conv_40_out, conv_41_w, conv_41_b, conv_41_out,
        RELU, conv_41_params.output_scale, true,
        tiled_matmul_type, check, "conv_41");

    return 0;
}
