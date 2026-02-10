#include "resnet_matmul_common.h"

int main(int argc, char* argv[]) {
    enum tiled_matmul_type_t tiled_matmul_type = get_tiled_matmul_type(argc, argv);
    bool check = false;

    static elem_t average[4][2048] row_align(1);

    tiled_matmul_nn_auto(fc_54_params.I, fc_54_params.J, fc_54_params.K,
        average, fc_54_w, fc_54_b, fc_54_out,
        NO_ACTIVATION, fc_54_params.output_scale, false,
        tiled_matmul_type, check, "fc_54");
    return 0;
}
