#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_11_params.I, conv_11_params.J, conv_11_params.K,
        conv_10_out, conv_11_w, conv_11_b, conv_11_out,
        NO_ACTIVATION, conv_11_params.output_scale, true,
        tiled_matmul_type, check, "conv_11");

    return 0;
}
