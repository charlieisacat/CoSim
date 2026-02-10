#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_7_params.I, conv_7_params.J, conv_7_params.K,
        conv_7_in, conv_7_w, conv_7_b, conv_7_out,
        RELU, conv_7_params.output_scale, true,
        tiled_matmul_type, check, "conv_7");

    return 0;
}
