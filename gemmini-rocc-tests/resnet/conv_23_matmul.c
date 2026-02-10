#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_23_params.I, conv_23_params.J, conv_23_params.K,
        conv_23_in, conv_23_w, conv_23_b, conv_23_out,
        RELU, conv_23_params.output_scale, true,
        tiled_matmul_type, check, "conv_23");

    return 0;
}
