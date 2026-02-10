#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_50_params.I, conv_50_params.J, conv_50_params.K,
        conv_49_out, conv_50_w, conv_50_b, conv_50_out,
        NO_ACTIVATION, conv_50_params.output_scale, true,
        tiled_matmul_type, check, "conv_50");

    return 0;
}
