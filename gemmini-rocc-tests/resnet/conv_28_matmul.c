#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_28_params.I, conv_28_params.J, conv_28_params.K,
        conv_28_in, conv_28_w, conv_28_b, conv_28_out,
        NO_ACTIVATION, conv_28_params.output_scale, true,
        tiled_matmul_type, check, "conv_28");

    return 0;
}
