#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_10_params.I, conv_10_params.J, conv_10_params.K,
        conv_10_in, conv_10_w, conv_10_b, conv_10_out,
        RELU, conv_10_params.output_scale, true,
        tiled_matmul_type, check, "conv_10");

    return 0;
}
