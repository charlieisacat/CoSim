#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_5_params.I, conv_5_params.J, conv_5_params.K,
        conv_5_in, conv_5_w, conv_5_b, conv_5_out,
        NO_ACTIVATION, conv_5_params.output_scale, true,
        tiled_matmul_type, check, "conv_5");

    return 0;
}
