#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_14_params.I, conv_14_params.J, conv_14_params.K,
        conv_13_out, conv_14_w, conv_14_b, conv_14_out,
        NO_ACTIVATION, conv_14_params.output_scale, true,
        tiled_matmul_type, check, "conv_14");

    return 0;
}
