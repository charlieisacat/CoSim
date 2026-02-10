#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_48_params.I, conv_48_params.J, conv_48_params.K,
        conv_46_out, conv_48_w, conv_48_b, conv_48_out,
        RELU, conv_48_params.output_scale, true,
        tiled_matmul_type, check, "conv_48");

    return 0;
}
