#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_40_params.I, conv_40_params.J, conv_40_params.K,
        conv_39_out, conv_40_w, conv_40_b, conv_40_out,
        NO_ACTIVATION, conv_40_params.output_scale, true,
        tiled_matmul_type, check, "conv_40");

    return 0;
}
