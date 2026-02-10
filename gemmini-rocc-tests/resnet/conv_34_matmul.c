#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_34_params.I, conv_34_params.J, conv_34_params.K,
        conv_33_out, conv_34_w, conv_34_b, conv_34_out,
        NO_ACTIVATION, conv_34_params.output_scale, true,
        tiled_matmul_type, check, "conv_34");

    return 0;
}
