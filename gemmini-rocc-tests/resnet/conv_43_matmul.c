#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    tiled_matmul_nn_auto(conv_43_params.I, conv_43_params.J, conv_43_params.K,
        conv_42_out, conv_43_w, conv_43_b, conv_43_out,
        NO_ACTIVATION, conv_43_params.output_scale, true,
        tiled_matmul_type, check, "conv_43");

    return 0;
}
