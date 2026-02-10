#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_20_params.I, conv_20_params.J, conv_20_params.K,
        conv_20_in, conv_20_w, conv_20_b, conv_20_out,
        RELU, conv_20_params.output_scale, true,
        tiled_matmul_type, check, "conv_20");

    return 0;
}
