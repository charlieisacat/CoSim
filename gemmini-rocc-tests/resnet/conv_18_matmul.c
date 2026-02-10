#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_18_params.I, conv_18_params.J, conv_18_params.K,
        conv_17_out, conv_18_w, conv_18_b, conv_18_out,
        NO_ACTIVATION, conv_18_params.output_scale, true,
        tiled_matmul_type, check, "conv_18");

    return 0;
}
