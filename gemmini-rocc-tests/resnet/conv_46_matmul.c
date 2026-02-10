#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_46_params.I, conv_46_params.J, conv_46_params.K,
        conv_45_out, conv_46_w, conv_46_b, conv_46_out,
        NO_ACTIVATION, conv_46_params.output_scale, true,
        tiled_matmul_type, check, "conv_46");

    return 0;
}
