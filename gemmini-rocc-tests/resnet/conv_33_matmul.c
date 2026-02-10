#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_33_params.I, conv_33_params.J, conv_33_params.K,
        conv_33_in, conv_33_w, conv_33_b, conv_33_out,
        RELU, conv_33_params.output_scale, true,
        tiled_matmul_type, check, "conv_33");

    return 0;
}
