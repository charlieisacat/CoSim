#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_32_params.I, conv_32_params.J, conv_32_params.K,
        conv_31_out, conv_32_w, conv_32_b, conv_32_out,
        RELU, conv_32_params.output_scale, true,
        tiled_matmul_type, check, "conv_32");

    return 0;
}
