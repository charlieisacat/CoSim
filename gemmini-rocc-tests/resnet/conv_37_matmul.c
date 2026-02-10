#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_37_params.I, conv_37_params.J, conv_37_params.K,
        conv_36_out, conv_37_w, conv_37_b, conv_37_out,
        NO_ACTIVATION, conv_37_params.output_scale, true,
        tiled_matmul_type, check, "conv_37");

    return 0;
}
