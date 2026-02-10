#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_47_params.I, conv_47_params.J, conv_47_params.K,
        conv_47_in, conv_47_w, conv_47_b, conv_47_out,
        NO_ACTIVATION, conv_47_params.output_scale, true,
        tiled_matmul_type, check, "conv_47");

    return 0;
}
