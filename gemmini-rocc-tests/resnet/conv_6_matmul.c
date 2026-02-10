#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_6_params.I, conv_6_params.J, conv_6_params.K,
        conv_4_out, conv_6_w, conv_6_b, conv_6_out,
        RELU, conv_6_params.output_scale, true,
        tiled_matmul_type, check, "conv_6");

    return 0;
}
