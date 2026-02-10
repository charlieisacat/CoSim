#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_25_params.I, conv_25_params.J, conv_25_params.K,
        conv_24_out, conv_25_w, conv_25_b, conv_25_out,
        RELU, conv_25_params.output_scale, true,
        tiled_matmul_type, check, "conv_25");

    return 0;
}
