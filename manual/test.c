#include "interface.h"

#include <stdio.h>
#include <stdlib.h>


// void ws_test_1(){
//     const tile_t M = 4;
//     const tile_t N = 4;
//     const tile_t K = 4;

//     const tile_t T_M = 2;
//     const tile_t T_N = 2;
//     const tile_t T_K = 2;

//     data_t* MK_matrix = (data_t*)calloc(M * K, sizeof(data_t));
//     data_t* KN_matrix = (data_t*)calloc(N * K, sizeof(data_t));
//     data_t* MN_matrix = (data_t*)calloc(M * N, sizeof(data_t));

//     if (!MK_matrix || !KN_matrix || !MN_matrix) {
//         free(MK_matrix);
//         free(KN_matrix);
//         free(MN_matrix);
//         return ;
//     }

//     cosim_config(T_N, T_K, T_M, MK_matrix, MN_matrix, KN_matrix, N, K, M);

//     int block_m = M / T_M;
//     int block_n = N / T_N;
//     int block_k = K / T_K;

//     for (int m = 0; m < block_m; m++) {
//         for (int n = 0; n < block_n; n++) {
//             int offset_to_MN = m * T_M * N + n * T_N;

//             for (int k = 0; k < block_k; k++) {
//                 int offset_to_MK = m * T_M * K + k * T_K;
//                 int offset_to_KN = n * T_N * K + k * T_K;

//                 printf("MK=%d KN=%d MN=%d\n", offset_to_MK, offset_to_KN, offset_to_MN);

//                 address_t address_MK = MK_matrix + offset_to_MK;
//                 address_t address_MN = MN_matrix + offset_to_MN;
//                 address_t address_KN = KN_matrix + offset_to_KN;

//                 cosim_preload(T_N,
//                               T_K,
//                               T_M,
//                               address_MK,
//                               address_MN,
//                               address_KN);

//                 cosim_matrix_multiply_flip(T_N,
//                                       T_K,
//                                       T_M,
//                                       address_MK,
//                                       address_MN,
//                                       address_KN);
//             }
//         }
//     }

//     cosim_fence();

//     free(MK_matrix);
//     free(KN_matrix);
//     free(MN_matrix);
// }

// void os_test_1() {
//     const tile_t M = 8;
//     const tile_t N = 8;
//     const tile_t K = 8;

//     const tile_t T_M = 4;
//     const tile_t T_N = 4;
//     const tile_t T_K = 8;

//     data_t* MK_matrix = (data_t*)calloc(M * K, sizeof(data_t));
//     data_t* KN_matrix = (data_t*)calloc(N * K, sizeof(data_t));
//     data_t* MN_matrix = (data_t*)calloc(M * N, sizeof(data_t));
    
//     cosim_config(T_N, T_K, T_M, MK_matrix, MN_matrix, KN_matrix, N, K, M);

//     int block_m = M / T_M;
//     int block_n = N / T_N;
//     int block_k = K / T_K;

//     for (int m = 0; m < block_m; m++) {
//         for (int n = 0; n < block_n; n++) {
//             int offset_to_MN = m * T_M * N + n * T_N;

//             for (int k = 0; k < block_k; k++) {
//                 int offset_to_MK = m * T_M * K + k * T_K;
//                 int offset_to_KN = n * T_N * K + k * T_K;

//                 address_t address_MK = MK_matrix + offset_to_MK;
//                 address_t address_MN = MN_matrix + offset_to_MN;
//                 address_t address_KN = KN_matrix + offset_to_KN;

//                 cosim_matrix_multiply(T_N,
//                                       T_K,
//                                       T_M,
//                                       address_MK,
//                                       address_MN,
//                                       address_KN);
//             }
//         }
//     }

//     cosim_fence();

//     free(MK_matrix);
//     free(KN_matrix);
//     free(MN_matrix);
// }


// // WS weight reuse & irregular
// void ws_test_2() 
// {
//     const tile_t M = 8;
//     const tile_t N = 8;
//     const tile_t K = 8;

//     const tile_t T_M = 4;
//     const tile_t T_N = 4;
//     const tile_t T_K = 4;

//     data_t* MK_matrix = (data_t*)calloc(M * K, sizeof(data_t));
//     data_t* KN_matrix = (data_t*)calloc(N * K, sizeof(data_t));
//     data_t* MN_matrix = (data_t*)calloc(M * N, sizeof(data_t));

//     int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
//     int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
//     int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

//     cosim_config(T_N, T_K, T_M, MK_matrix, MN_matrix, KN_matrix, N, K, M);

//     for (int k = 0; k < block_k; k++) {
//         for (int n = 0; n < block_n; n++) {
//             int offset_to_KN = n * T_N * K + k * T_K;

//             int valid_T_K = T_K;
//             int valid_T_N = T_N;

//             if(k == block_k -1 && (K % T_K != 0)) {
//                 valid_T_K = K % T_K;
//             }

//             if(n == block_n -1 && (N % T_N != 0)) {
//                 valid_T_N = N % T_N;
//             }

//             cosim_preload(valid_T_N, 
//                           valid_T_K, 
//                           0, 
//                           0, 
//                           0, 
//                           KN_matrix + offset_to_KN);

//             for (int m = 0; m < block_m; m++) {
//                 int offset_to_MK = m * T_M * K + k * T_K;
//                 int offset_to_MN = m * T_M * N + n * T_N;

//                 int valid_T_M = T_M;
//                 if(m == block_m -1 && (M % T_M != 0)) {
//                     valid_T_M = M % T_M;
//                 }

//                 if(m == 0) {
//                     cosim_matrix_multiply_flip(valid_T_N,
//                                                valid_T_K,
//                                                valid_T_M,
//                                                MK_matrix + offset_to_MK,
//                                                MN_matrix + offset_to_MN,
//                                                KN_matrix + offset_to_KN);
//                 } else {
//                     cosim_matrix_multiply( valid_T_N,
//                                            valid_T_K,
//                                            valid_T_M,
//                                            MK_matrix + offset_to_MK,
//                                            MN_matrix + offset_to_MN,
//                                            KN_matrix + offset_to_KN);
//                 }
//             }
//         }
//     }

//     cosim_fence();

//     free(MK_matrix);
//     free(KN_matrix);
//     free(MN_matrix);
// }


// void ws_test_3()
// {

//     const tile_t M = 9;
//     const tile_t N = 15;
//     const tile_t K = 17;

//     const tile_t T_M = 4;
//     const tile_t T_N = 4;
//     const tile_t T_K = 4;

//     data_t* MK_matrix = (data_t*)calloc(M * K, sizeof(data_t));
//     data_t* KN_matrix = (data_t*)calloc(N * K, sizeof(data_t));
//     data_t* MN_matrix = (data_t*)calloc(M * N, sizeof(data_t));

//     int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
//     int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
//     int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

//     cosim_config(T_N, T_K, T_M, MK_matrix, MN_matrix, KN_matrix, N, K, M);
//     // cosim_config(T_N, T_K, T_M, MK_matrix, MN_matrix, KN_matrix, 0,0,0);

//     for (int m = 0; m < block_m; m++) {
//         for (int n = 0; n < block_n; n++) {
//             int offset_to_MN = m * T_M * N + n * T_N;

//             int valid_T_M = T_M;
//             int valid_T_N = T_N;
//             if(m == block_m -1 && (M % T_M != 0)) {
//                 valid_T_M = M % T_M;
//             }
//             if(n == block_n -1 && (N % T_N != 0)) {
//                 valid_T_N = N % T_N;
//             }

//             for (int k = 0; k < block_k; k++) {
//                 int offset_to_MK = m * T_M * K + k * T_K;
//                 int offset_to_KN = n * T_N * K + k * T_K;

//                 int valid_T_K = T_K;
//                 if(k == block_k -1 && (K % T_K != 0)) {
//                     valid_T_K = K % T_K;
//                 }

//                 cosim_preload( valid_T_N,
//                                valid_T_K,
//                                valid_T_M,
//                                MK_matrix + offset_to_MK,
//                                MN_matrix + offset_to_MN,
//                                KN_matrix + offset_to_KN);
                
//                 cosim_matrix_multiply_flip( valid_T_N,
//                                             valid_T_K,
//                                             valid_T_M,
//                                             MK_matrix + offset_to_MK,
//                                             MN_matrix + offset_to_MN,
//                                             KN_matrix + offset_to_KN);

//             }
//         }
//     }
//     cosim_fence();

//     free(MK_matrix);
//     free(KN_matrix);
//     free(MN_matrix);

// }


// reuse output & irregular
void os_test_2() 
{
    const tile_t M = 784;
    const tile_t N = 256;
    const tile_t K = 2304;

    const tile_t T_M = 16;
    const tile_t T_N = 16;
    const tile_t T_K = 16;

    data_t* MK_matrix = (data_t*)calloc(M * K, sizeof(data_t));
    data_t* KN_matrix = (data_t*)calloc(N * K, sizeof(data_t));
    data_t* MN_matrix = (data_t*)calloc(M * N, sizeof(data_t));

    int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
    int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
    int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

    cosim_config(T_N, T_K, T_M, (uint32_t)MK_matrix, (uint32_t)MN_matrix, (uint32_t)KN_matrix, N, K, M);

    for (int m = 0; m < block_m; m++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_MN = m * T_M * N + n * T_N;

            int valid_T_M = T_M;
            if(m == block_m -1 && (M % T_M != 0)) {
                valid_T_M = M % T_M;
            }

            int valid_T_N = T_N;
            if(n == block_n -1 && (N % T_N != 0)) {
                valid_T_N = N % T_N;
            }   

            for (int k = 0; k < block_k; k++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_KN = n * T_N * K + k * T_K;

                int valid_T_K = T_K;
                if(k == block_k -1 && (K % T_K != 0)) {
                    valid_T_K = K % T_K;
                }

                if(k == block_k -1) {
                    cosim_matrix_multiply(valid_T_N,
                                          valid_T_K,
                                          valid_T_M,
                                          (uint32_t)MK_matrix + offset_to_MK,
                                          (uint32_t)MN_matrix + offset_to_MN,
                                          (uint32_t)KN_matrix + offset_to_KN,
                                          (uint32_t)MK_matrix + offset_to_MK,
                                          (uint32_t)MN_matrix + offset_to_MN,
                                          (uint32_t)KN_matrix + offset_to_KN
                    );
                } else {
                    cosim_matrix_multiply_stay(valid_T_N,
                                               valid_T_K,
                                               valid_T_M,
                                               (uint32_t)MK_matrix + offset_to_MK,
                                               (uint32_t)MN_matrix + offset_to_MN,
                                               (uint32_t)KN_matrix + offset_to_KN,
                                               (uint32_t)MK_matrix + offset_to_MK,
                                               (uint32_t)MN_matrix + offset_to_MN,
                                               (uint32_t)KN_matrix + offset_to_KN
                                            );
                }
            }
        }
    }

    cosim_fence();

    free(MK_matrix);
    free(KN_matrix);
    free(MN_matrix);
}


// // only mm & irregular
// void os_test_3() 
// {
//     const tile_t M = 9;
//     const tile_t N = 15;
//     const tile_t K = 17;

//     const tile_t T_M = 4;
//     const tile_t T_N = 4;
//     const tile_t T_K = 4;

//     data_t* MK_matrix = (data_t*)calloc(M * K, sizeof(data_t));
//     data_t* KN_matrix = (data_t*)calloc(N * K, sizeof(data_t));
//     data_t* MN_matrix = (data_t*)calloc(M * N, sizeof(data_t));

//     int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
//     int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
//     int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

//     cosim_config(T_N, T_K, T_M, MK_matrix, MN_matrix, KN_matrix, N, K, M);

//     for (int m = 0; m < block_m; m++) {
//         for (int n = 0; n < block_n; n++) {
//             int offset_to_MN = m * T_M * N + n * T_N;

//             int valid_T_M = T_M;
//             if(m == block_m -1 && (M % T_M != 0)) {
//                 valid_T_M = M % T_M;    
//             }

//             int valid_T_N = T_N;
//             if(n == block_n -1 && (N % T_N != 0)) {
//                 valid_T_N = N % T_N;    
//             }

//             for (int k = 0; k < block_k; k++) {
//                 int offset_to_MK = m * T_M * K + k * T_K;
//                 int offset_to_KN = n * T_N * K + k * T_K;

//                 // std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

//                 int valid_T_K = T_K;
//                 if(k == block_k -1 && (K % T_K != 0)) {
//                     valid_T_K = K % T_K;
//                 }

//                 cosim_matrix_multiply( valid_T_N,
//                                        valid_T_K,
//                                        valid_T_M,
//                                        MK_matrix + offset_to_MK,
//                                        MN_matrix + offset_to_MN,
//                                        KN_matrix + offset_to_KN);

//                 // Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
//                 // mm->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN);

//                 // stonne_instance->get_sac()->receive(mm);
//             }
//         }
//     }

//     cosim_fence();

//     free(MK_matrix);
//     free(KN_matrix);
//     free(MN_matrix);
// }

int main(void) {
    // os_test_1();
    // os_test_2();
    os_test_2();
    // ws_test_1();
    // ws_test_2();
    // ws_test_3();
    return 0;
}
