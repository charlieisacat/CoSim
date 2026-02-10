#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_30_params.I, conv_30_params.J, conv_30_params.K,
        conv_30_in, conv_30_w, conv_30_b, conv_30_out,
        RELU, conv_30_params.output_scale, true,
        tiled_matmul_type, check, "conv_30");

    return 0;
}
