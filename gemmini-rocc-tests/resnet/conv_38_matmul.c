#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_38_params.I, conv_38_params.J, conv_38_params.K,
        conv_37_out, conv_38_w, conv_38_b, conv_38_out,
        RELU, conv_38_params.output_scale, true,
        tiled_matmul_type, check, "conv_38");

    return 0;
}
