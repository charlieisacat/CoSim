#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_8_params.I, conv_8_params.J, conv_8_params.K,
        conv_7_out, conv_8_w, conv_8_b, conv_8_out,
        NO_ACTIVATION, conv_8_params.output_scale, true,
        tiled_matmul_type, check, "conv_8");

    return 0;
}
