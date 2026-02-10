#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_1_params.I, conv_1_params.J, conv_1_params.K,
        conv_1_in, conv_1_w, conv_1_b, conv_1_out,
        RELU, conv_1_params.output_scale, true,
        tiled_matmul_type, check, "conv_1");

    return 0;
}
