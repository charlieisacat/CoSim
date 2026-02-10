#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_52_params.I, conv_52_params.J, conv_52_params.K,
        conv_52_in, conv_52_w, conv_52_b, conv_52_out,
        RELU, conv_52_params.output_scale, true,
        tiled_matmul_type, check, "conv_52");

    return 0;
}
