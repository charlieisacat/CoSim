#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_35_params.I, conv_35_params.J, conv_35_params.K,
        conv_34_out, conv_35_w, conv_35_b, conv_35_out,
        RELU, conv_35_params.output_scale, true,
        tiled_matmul_type, check, "conv_35");

    return 0;
}
