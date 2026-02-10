#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_51_params.I, conv_51_params.J, conv_51_params.K,
        conv_50_out, conv_51_w, conv_51_b, conv_51_out,
        RELU, conv_51_params.output_scale, true,
        tiled_matmul_type, check, "conv_51");

    return 0;
}
