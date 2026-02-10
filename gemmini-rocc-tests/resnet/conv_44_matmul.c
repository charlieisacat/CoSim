#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_44_params.I, conv_44_params.J, conv_44_params.K,
        conv_43_out, conv_44_w, conv_44_b, conv_44_out,
        RELU, conv_44_params.output_scale, true,
        tiled_matmul_type, check, "conv_44");

    return 0;
}
