#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_31_params.I, conv_31_params.J, conv_31_params.K,
        conv_30_out, conv_31_w, conv_31_b, conv_31_out,
        NO_ACTIVATION, conv_31_params.output_scale, true,
        tiled_matmul_type, check, "conv_31");

    return 0;
}
