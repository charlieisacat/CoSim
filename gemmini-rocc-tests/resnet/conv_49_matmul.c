#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_49_params.I, conv_49_params.J, conv_49_params.K,
        conv_49_in, conv_49_w, conv_49_b, conv_49_out,
        RELU, conv_49_params.output_scale, true,
        tiled_matmul_type, check, "conv_49");

    return 0;
}
