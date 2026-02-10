#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_27_params.I, conv_27_params.J, conv_27_params.K,
        conv_26_out, conv_27_w, conv_27_b, conv_27_out,
        NO_ACTIVATION, conv_27_params.output_scale, true,
        tiled_matmul_type, check, "conv_27");

    return 0;
}
