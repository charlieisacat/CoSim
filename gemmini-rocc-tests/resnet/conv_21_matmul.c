#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_21_params.I, conv_21_params.J, conv_21_params.K,
        conv_20_out, conv_21_w, conv_21_b, conv_21_out,
        NO_ACTIVATION, conv_21_params.output_scale, true,
        tiled_matmul_type, check, "conv_21");

    return 0;
}
