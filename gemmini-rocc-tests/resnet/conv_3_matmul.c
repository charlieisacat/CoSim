#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_3_params.I, conv_3_params.J, conv_3_params.K,
        conv_3_in, conv_3_w, conv_3_b, conv_3_out,
        RELU, conv_3_params.output_scale, true,
        tiled_matmul_type, check, "conv_3");

    return 0;
}
