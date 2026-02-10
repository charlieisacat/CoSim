#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_29_params.I, conv_29_params.J, conv_29_params.K,
        conv_27_out, conv_29_w, conv_29_b, conv_29_out,
        RELU, conv_29_params.output_scale, true,
        tiled_matmul_type, check, "conv_29");

    return 0;
}
