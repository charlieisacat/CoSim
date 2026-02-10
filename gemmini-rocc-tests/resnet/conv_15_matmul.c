#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_15_params.I, conv_15_params.J, conv_15_params.K,
        conv_15_in, conv_15_w, conv_15_b, conv_15_out,
        NO_ACTIVATION, conv_15_params.output_scale, true,
        tiled_matmul_type, check, "conv_15");

    return 0;
}
