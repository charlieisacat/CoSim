#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_9_params.I, conv_9_params.J, conv_9_params.K,
        conv_8_out, conv_9_w, conv_9_b, conv_9_out,
        RELU, conv_9_params.output_scale, true,
        tiled_matmul_type, check, "conv_9");

    return 0;
}
