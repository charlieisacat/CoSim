#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_22_params.I, conv_22_params.J, conv_22_params.K,
        conv_21_out, conv_22_w, conv_22_b, conv_22_out,
        RELU, conv_22_params.output_scale, true,
        tiled_matmul_type, check, "conv_22");

    return 0;
}
