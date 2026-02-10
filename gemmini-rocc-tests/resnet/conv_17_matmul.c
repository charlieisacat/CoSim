#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_17_params.I, conv_17_params.J, conv_17_params.K,
        conv_17_in, conv_17_w, conv_17_b, conv_17_out,
        RELU, conv_17_params.output_scale, true,
        tiled_matmul_type, check, "conv_17");

    return 0;
}
