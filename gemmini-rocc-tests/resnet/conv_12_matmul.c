#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_12_params.I, conv_12_params.J, conv_12_params.K,
        conv_11_out, conv_12_w, conv_12_b, conv_12_out,
        RELU, conv_12_params.output_scale, true,
        tiled_matmul_type, check, "conv_12");

    return 0;
}
