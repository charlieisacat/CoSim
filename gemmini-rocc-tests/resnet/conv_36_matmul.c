#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_36_params.I, conv_36_params.J, conv_36_params.K,
        conv_36_in, conv_36_w, conv_36_b, conv_36_out,
        RELU, conv_36_params.output_scale, true,
        tiled_matmul_type, check, "conv_36");

    return 0;
}
