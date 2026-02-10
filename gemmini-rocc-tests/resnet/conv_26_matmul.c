#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_26_params.I, conv_26_params.J, conv_26_params.K,
        conv_26_in, conv_26_w, conv_26_b, conv_26_out,
        RELU, conv_26_params.output_scale, true,
        tiled_matmul_type, check, "conv_26");

    return 0;
}
