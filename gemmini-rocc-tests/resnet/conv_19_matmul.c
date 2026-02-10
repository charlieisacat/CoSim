#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_19_params.I, conv_19_params.J, conv_19_params.K,
        conv_18_out, conv_19_w, conv_19_b, conv_19_out,
        RELU, conv_19_params.output_scale, true,
        tiled_matmul_type, check, "conv_19");

    return 0;
}
