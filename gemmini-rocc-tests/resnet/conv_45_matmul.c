#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_45_params.I, conv_45_params.J, conv_45_params.K,
        conv_45_in, conv_45_w, conv_45_b, conv_45_out,
        RELU, conv_45_params.output_scale, true,
        tiled_matmul_type, check, "conv_45");

    return 0;
}
