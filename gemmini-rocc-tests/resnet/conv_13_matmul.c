#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_13_params.I, conv_13_params.J, conv_13_params.K,
        conv_13_in, conv_13_w, conv_13_b, conv_13_out,
        RELU, conv_13_params.output_scale, true,
        tiled_matmul_type, check, "conv_13");

    return 0;
}
