#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_24_params.I, conv_24_params.J, conv_24_params.K,
        conv_23_out, conv_24_w, conv_24_b, conv_24_out,
        NO_ACTIVATION, conv_24_params.output_scale, true,
        tiled_matmul_type, check, "conv_24");

    return 0;
}
