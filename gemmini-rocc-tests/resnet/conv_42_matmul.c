#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_42_params.I, conv_42_params.J, conv_42_params.K,
        conv_42_in, conv_42_w, conv_42_b, conv_42_out,
        RELU, conv_42_params.output_scale, true,
        tiled_matmul_type, check, "conv_42");

    return 0;
}
