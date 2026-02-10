#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_2_params.I, conv_2_params.J, conv_2_params.K,
        conv_2_in, conv_2_w, conv_2_b, conv_2_out,
        RELU, conv_2_params.output_scale, true,
        tiled_matmul_type, check, "conv_2");

    return 0;
}
