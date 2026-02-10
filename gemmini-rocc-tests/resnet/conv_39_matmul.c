#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_39_params.I, conv_39_params.J, conv_39_params.K,
        conv_39_in, conv_39_w, conv_39_b, conv_39_out,
        RELU, conv_39_params.output_scale, true,
        tiled_matmul_type, check, "conv_39");

    return 0;
}
