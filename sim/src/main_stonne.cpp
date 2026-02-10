#include <iostream>
#include "SA/STONNEModel.h"
#include "SA/types.h"
#include <chrono>
#include <assert.h>
#include "SA/testbench.h"
#include <string>
#include <math.h>
#include "SA/utility.h"
#include <cstring>

using namespace std;

struct NPUConfig
{
    unsigned int ms_rows;
    unsigned int ms_cols;
    unsigned int dn_bw;
    unsigned int rn_bw;
    unsigned int T_N;
    unsigned int T_M;
    unsigned int T_K;
    ReduceNetwork_t rn_type;
    MultiplierNetwork_t mn_type;
    MemoryController_t mem_ctrl;

    // delete
    unsigned int M;
    unsigned int N;
    unsigned int K;

    void *input_address;
    void *filter_address;
    void *output_address;

    unsigned int accumulation_buffer_enabled;


    DataflowType dataflow;

    NPUConfig(unsigned int ms_rows,
              unsigned int ms_cols,
              unsigned int dn_bw,
              unsigned int rn_bw,
              unsigned int T_N,
              unsigned int T_M,
              unsigned int T_K,
              ReduceNetwork_t rn_type,
              MultiplierNetwork_t mn_type,
              MemoryController_t mem_ctrl,
              unsigned int M,
              unsigned int N,
              unsigned int K,
              unsigned int accumulation_buffer_enabled, DataflowType _dataflow)
        : ms_rows(ms_rows), ms_cols(ms_cols), dn_bw(dn_bw), rn_bw(rn_bw),
          T_N(T_N), T_M(T_M), T_K(T_K), rn_type(rn_type), mn_type(mn_type), mem_ctrl(mem_ctrl),
          M(M), N(N), K(K), accumulation_buffer_enabled(accumulation_buffer_enabled), dataflow(_dataflow)
    {
    }
};

void configDenseGEMMParameters(Config &stonne_cfg, NPUConfig *npuConfig);
bool runDenseGEMMCommand(NPUConfig *npuConfig);
void printDenseMatrix(float *matrix, unsigned int rows, unsigned int cols);
void printBitMap(unsigned int *bitmap, unsigned int rows, unsigned int cols);

// TODO:
// In WS, T_K should be equal or less than ms_rows
// and T_N should be equal or less than ms_cols
//, because weights are stored in SA

// In OS, T_N should be equal or less than ms_cols (from top)
// T_M should be equal or less than ms_rows (from left)

#if 0
int main(int argc, char *argv[])
{
    // NPUConfig *npuConfig = new NPUConfig(4, 4, 8, 16, 4, 4, 4, TEMPORALRN, WS_MESH, TPU_OS_DENSE, 9, 15, 17, true, DATAFLOW_WS);
    // NPUConfig *npuConfig = new NPUConfig(2, 2, 4, 8, 2, 2, 2, TEMPORALRN, WS_MESH, TPU_OS_DENSE, 3, 3, 3, true, DATAFLOW_WS);
    // NPUConfig *npuConfig = new NPUConfig(2, 2, 4, 8, 2, 2, 2, TEMPORALRN, OS_MESH, TPU_OS_DENSE, 3, 3, 3, true, DATAFLOW_OS);
    // NPUConfig *npuConfig = new NPUConfig(2, 2, 4, 8, 2, 2, 2, TEMPORALRN, OS_MESH, TPU_OS_DENSE, 3, 3, 3, true, DATAFLOW_OS);
    NPUConfig *npuConfig = new NPUConfig(4, 4, 8, 16, 4, 4, 4, TEMPORALRN, OS_MESH, TPU_OS_DENSE, 9, 15, 17, true, DATAFLOW_OS);
    // NPUConfig *npuConfig = new NPUConfig(16, 16, 32, 64, 16, 16, 16, TEMPORALRN, OS_MESH, TPU_OS_DENSE, 16, 16, 3, true, DATAFLOW_OS);

    // NPUConfig *npuConfig = new NPUConfig(2, 2, 4, 8, 2, 2, 2, TEMPORALRN, WS_MESH, TPU_OS_DENSE, 4, 4, 4, true, DATAFLOW_WS);
    // NPUConfig *npuConfig = new NPUConfig(4, 4, 8, 16, 4, 4, 4, TEMPORALRN, WS_MESH, TPU_OS_DENSE, 5, 5, 5, true, DATAFLOW_WS);
    // NPUConfig *npuConfig = new NPUConfig(2, 2, 4, 8, 2, 2, 4, TEMPORALRN, OS_MESH, TPU_OS_DENSE, 4, 4, 4, true, DATAFLOW_OS);
    // NPUConfig *npuConfig = new NPUConfig(4, 4, 8, 16, 4, 4, 4, TEMPORALRN, OS_MESH, TPU_OS_DENSE, 8, 8, 8, true, DATAFLOW_OS);
    runDenseGEMMCommand(npuConfig);
}
#endif

data_t* pad(data_t *data, int rows, int cols, int pad_rows, int pad_cols) {
    int new_rows = rows + pad_rows;
    int new_cols = cols + pad_cols;
    data_t* padded_data = new data_t[new_rows * new_cols];
    memset(padded_data, 0, new_rows * new_cols * sizeof(data_t));

    // cpy with memcop
    for(int r = 0; r < rows; r++) {
        std::cout<<"r = "<<r << " offset="<< r * new_cols<<" from "<<r * cols <<std::endl;  
        memcpy(&padded_data[r * new_cols], &data[r * cols], cols * sizeof(data_t));
    }

    return padded_data;
}

data_t* unpad(data_t *data, int rows, int cols, int pad_rows, int pad_cols) {
    int new_rows = rows - pad_rows;
    int new_cols = cols - pad_cols;
    data_t* unpadded_data = new data_t[new_rows * new_cols];
    memset(unpadded_data, 0, new_rows * new_cols * sizeof(data_t));

    // cpy with memcop
    for(int r = 0; r < new_rows; r++) {
        memcpy(&unpadded_data[r * new_cols], &data[r * cols], new_cols * sizeof(data_t));
    }

    return unpadded_data;
}

bool runDenseGEMMCommand(NPUConfig *npuConfig)
{
    float EPSILON = 0.05;
    unsigned int MAX_RANDOM = 10; // Variable used to generate the random values

    // Tile parameters
    unsigned int T_M = npuConfig->T_M;
    unsigned int T_K = npuConfig->T_K;
    unsigned int T_N = npuConfig->T_N;

    std::cout<<"T_M"<<T_M<<" T_K"<<T_K<<" T_N"<<T_N<<std::endl;

    Config stonne_cfg;                                // Hardware parameters
    configDenseGEMMParameters(stonne_cfg, npuConfig); // Modify stonne_cfg and the variables according to user arguments

    unsigned int M = npuConfig->M;
    unsigned int N = npuConfig->N;
    unsigned int K = npuConfig->K;

    unsigned int pad_M = T_M - (M % T_M == 0 ? T_M : M % T_M);
    unsigned int pad_N = T_N - (N % T_N == 0 ? T_N : N % T_N);
    unsigned int pad_K = T_K - (K % T_K == 0 ? T_K : K % T_K);

    std::cout<<"pad_M"<<pad_M<<" pad_N"<<pad_N<<" pad_K"<<pad_K<<std::endl;

    // Creating arrays to store the matrices
    unsigned int MK_size = M * K;
    unsigned int KN_size = N * K;
    // unsigned int output_size = (M + pad_M) * (N + pad_N);
    unsigned int output_size = M * N;
    data_t *MK_matrix = new data_t[MK_size];
    data_t *KN_matrix = new data_t[KN_size];
    data_t *output_cpu = new data_t[output_size]; // Used to store the CPU computed values to compare with the simulator version
    data_t* accbuf = new data_t[M * N];

    data_t* D = new data_t[M * N];

    // MUST init with 0
    memset(accbuf, 0, M * N * sizeof(data_t));
    memset(D, 0, M * N * sizeof(data_t));

    stonne_cfg.print_stats_enabled = false;

    // Filling the arrays with random values
    for (int i = 0; i < MK_size; i++)
    {
        MK_matrix[i] = rand() % MAX_RANDOM + 1;
    }

    for (int i = 0; i < KN_size; i++)
    {
        KN_matrix[i] = rand() % MAX_RANDOM + 1;
    }

    // accbuf = pad(accbuf, M, N, pad_M, pad_N);
    // MK_matrix = pad(MK_matrix, M, K, pad_M, pad_K);
    // KN_matrix = pad(KN_matrix, N, K, pad_N, pad_K);

    // M += pad_M;
    // N += pad_N;
    // K += pad_K;

    //print MK
    std::cout << "MK matrix: " << std::endl;
    for (int i = 0; i < M ; i++)
    {
        for (int j = 0; j < K ; j++)
        {   
            std::cout << MK_matrix[i * (K) + j] << " ";
        }
        std::cout << std::endl;
    }

    //print NK
    std::cout << "KN matrix: " << std::endl;
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < K ; j++)
        {
            std::cout << KN_matrix[i * (K) + j] << " ";
        }
        std::cout << std::endl;
    }

    sequential_layer(1, K, 1, N, 1, M, 1, K, 1, MK_matrix, KN_matrix, output_cpu); // Supposes that MK=inputs (M=batch size) and KN=filters (N=number of filters)

    for (int i = 0; i < M; i++)
    {
        for (int j = 0; j < N; j++)
        {
            std::cout << output_cpu[i * N + j] << " ";
        }
        std::cout << std::endl;
    }

    Stonne *stonne_instance = new Stonne(stonne_cfg); // Creating instance of the simulator

    // stonne_instance->configMem((address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, M, N, K, T_M, T_N, T_K);
    
    // WS weight reuse & irregular
#if 0
    int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
    int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
    int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

    int idx = 0;
    Command *conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

    // m=0, n=0, k=0
    // m=1, n=0, k=0
    // m=0, n=1, k=0
    // m=1, n=1, k=0
    for (int k = 0; k < block_k; k++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_KN = n * T_N * K + k * T_K;

            Command* pre = new Command(idx++, EXE, PRELOAD);

            int valid_T_K = T_K;
            int valid_T_N = T_N;

            if(k == block_k -1 && (K % T_K != 0)) {
                valid_T_K = K % T_K;
            }

            if(n == block_n -1 && (N % T_N != 0)) {
                valid_T_N = N % T_N;
            }
            pre->set_params(valid_T_N, valid_T_K, 0, 0, 0, offset_to_KN);
            stonne_instance->get_sac()->receive(pre);

            for (int m = 0; m < block_m; m++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_MN = m * T_M * N + n * T_N;

                std::cout << "MK=" << offset_to_MK << " KN=" << offset_to_KN
                          << " MN=" << offset_to_MN << std::endl;

                Command *mm = nullptr;

                if(m == 0) {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
                } else {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                }

                int valid_T_M = T_M;
                if(m == block_m -1 && (M % T_M != 0)) {
                    valid_T_M = M % T_M;
                }

                mm->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN);
                stonne_instance->get_sac()->receive(mm);
            }
        }
    }

#endif

    // WS weight reuse
#if 0

    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;
    Command *conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

    // m=0, n=0, k=0
    // m=1, n=0, k=0
    // m=0, n=1, k=0
    // m=1, n=1, k=0
    for (int k = 0; k < block_k; k++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_KN = n * T_N * K + k * T_K;

            Command* pre = new Command(idx++, EXE, PRELOAD);
            pre->set_params(T_N, T_K, T_M, 0, 0, offset_to_KN);
            stonne_instance->get_sac()->receive(pre);

            for (int m = 0; m < block_m; m++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_MN = m * T_M * N + n * T_N;

                std::cout << "MK=" << offset_to_MK << " KN=" << offset_to_KN
                          << " MN=" << offset_to_MN << std::endl;

                Command *mm = nullptr;

                if(m == 0) {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
                } else {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                }

                mm->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);
                stonne_instance->get_sac()->receive(mm);
            }
        }
    }

#endif

    // WS & irregular
#if 0
    int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
    int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
    int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

    for (int m = 0; m < block_m; m++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_MN = m * T_M * N + n * T_N;

            int valid_T_M = T_M;
            int valid_T_N = T_N;
            if(m == block_m -1 && (M % T_M != 0)) {
                valid_T_M = M % T_M;
            }
            if(n == block_n -1 && (N % T_N != 0)) {
                valid_T_N = N % T_N;
            }

            for (int k = 0; k < block_k; k++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_KN = n * T_N * K + k * T_K;

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                int valid_T_K = T_K;
                if(k == block_k -1 && (K % T_K != 0)) {
                    valid_T_K = K % T_K;
                }

                Command* pre = new Command(idx++, EXE, PRELOAD);
                Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);

                pre->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN);
                mm->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                stonne_instance->get_sac()->receive(pre);
                stonne_instance->get_sac()->receive(mm);
            }
        }
    }
#endif


    // WS
#if 0
    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

    for (int m = 0; m < block_m; m++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_MN = m * T_M * N + n * T_N;

            for (int k = 0; k < block_k; k++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_KN = n * T_N * K + k * T_K;

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                Command* pre = new Command(idx++, EXE, PRELOAD);
                Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);

                pre->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);
                mm->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                // pre->insert_child(mm);
                // mm->insert_parent(pre);

#if 0
                if(stonne_instance->get_sac()->get_inst_queue_size() > 0) {
                    Command* back = stonne_instance->get_sac()->get_inst_queue_back();
                    back->insert_child(pre);
                    pre->insert_parent(back);
                }
#endif

                stonne_instance->get_sac()->receive(pre);
                stonne_instance->get_sac()->receive(mm);
            }
        }
    }
#endif

// interleave mm and pre, irregular
#if 0
    int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
    int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
    int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);
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

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                Command *back = nullptr;
                Command *mm = nullptr;

                if (stonne_instance->get_sac()->get_inst_queue_size() > 0) {
                  back =stonne_instance->get_sac()->get_inst_queue_back();
                }

                if(back && back->get_op_type() == PRELOAD) {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
                } else {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                }

                // Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                mm->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                stonne_instance->get_sac()->receive(mm);
                if (k < block_k - 1) {
                    Command* pre = new Command(idx++, EXE, PRELOAD);

                    std::cout <<"idx="<<idx - 1 << " m=" << m << " n=" << n << " k=" << k
                              << " preload: " << offset_to_MN << std::endl;
                    pre->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                    mm->insert_child(pre);
                    pre->insert_parent(mm);

                    stonne_instance->get_sac()->receive(pre);
                    
                    std::cout<< "pre:"<<pre->get_seq_num()<< " depends "<<mm->get_seq_num()<<std::endl;
                }
            }
        }
    }
#endif

// interleave mm and pre
#if 0
    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);
    for (int m = 0; m < block_m; m++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_MN = m * T_M * N + n * T_N;

            for (int k = 0; k < block_k; k++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_KN = n * T_N * K + k * T_K;

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                Command *back = nullptr;
                Command *mm = nullptr;

                if (stonne_instance->get_sac()->get_inst_queue_size() > 0) {
                  back =stonne_instance->get_sac()->get_inst_queue_back();
                }

                if(back && back->get_op_type() == PRELOAD) {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
                } else {
                    mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                }

                // Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                mm->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                stonne_instance->get_sac()->receive(mm);
                if (k < block_k - 1) {
                    Command* pre = new Command(idx++, EXE, PRELOAD);

                    std::cout <<"idx="<<idx - 1 << " m=" << m << " n=" << n << " k=" << k
                              << " preload: " << offset_to_MN << std::endl;
                    pre->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                    mm->insert_child(pre);
                    pre->insert_parent(mm);

                    stonne_instance->get_sac()->receive(pre);
                    
                    std::cout<< "pre:"<<pre->get_seq_num()<< " depends "<<mm->get_seq_num()<<std::endl;
                }
            }
        }
    }
#endif

// reuse output
#if 0
    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

    for (int m = 0; m < block_m; m++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_MN = m * T_M * N + n * T_N;

            for (int k = 0; k < block_k; k++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_KN = n * T_N * K + k * T_K;

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                mm->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);
                mm->set_stay(true); // stay in SA

                 if(k == block_k -1) {
                    mm->set_stay(false); // last mm do not stay
                 }
                
                stonne_instance->get_sac()->receive(mm);
            }
        }
        // break;
    }
#endif

// reuse output & irregular
#if 1
    int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
    int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
    int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

    uint64_t sp_a_start = 0;
    uint64_t dram_a_start = (uint64_t)MK_matrix;
    
    uint64_t sp_b_start = block_m * block_k * T_M; // after A
    uint64_t dram_b_start = (uint64_t)KN_matrix;
    
    uint64_t sp_c_start = 0;
    uint64_t dram_c_start = (uint64_t)accbuf;

    uint64_t sp_d_start = 0;
    uint64_t dram_d_start = (uint64_t)D;

    // TODO: block_per_row should be less than or equal to bank num
    int block_per_row = min(2, block_k);
    int N_blocks = min(2, block_n);
    bool repeating_bias = true;

    // Move-in D
    std::cout<<"Move-in D"<<"\n";
    for (size_t m = 0; m < block_m; m++) {
      for (size_t n = 0; n < block_n; n += N_blocks) {
        const size_t bias_row = repeating_bias ? 0 : m;
        uint64_t dram_d_addr = dram_d_start + bias_row * T_M * K + n * T_N;
        uint64_t sp_d_addr = sp_d_start + m * block_n * T_M + n * T_M; 

        tile_t cols = N_blocks * T_N - ((n + N_blocks >= block_n) ? pad_N : 0);
        tile_t rows = T_M - ((m == block_m - 1) ? pad_M : 0);

        // std::cout<<"Move D from DRAM "<<dram_d_addr<<" to SP "<<sp_d_addr
        //          <<" rows "<<rows<<" cols "<<cols<<std::endl;

        Command *move_d = new Command(idx++, LOAD_ACC, MOVE_IN_ACC);
        move_d->set_params(dram_d_addr, sp_d_addr, cols, rows);
        stonne_instance->get_sac()->receive(move_d);
      }
    }

    std::cout<<"Move-in A"<<"\n";
    // Move-in A
    for (int m = 0; m < block_m; m++) {
        for (int k = 0; k < block_k; k += block_per_row) {
            uint64_t dram_a_addr = dram_a_start + m * T_M * K + k * T_K;
            uint64_t sp_a_addr = sp_a_start + m * block_k * T_M + k * T_M;

            tile_t cols = block_per_row * T_K - ((k + block_per_row >= block_k) ? pad_K : 0);
            tile_t rows = T_M - ((m == block_m -1) ? pad_M : 0);

            // std::cout<<"Move A from DRAM "<<dram_a_addr<<" to SP "<<sp_a_addr
            //          <<" rows "<<rows<<" cols "<<cols<<std::endl;

            Command *move_a = new Command(idx++, LOAD, MOVE_IN);
            move_a->set_params(dram_a_addr, sp_a_addr, cols, rows);
            stonne_instance->get_sac()->receive(move_a);
        }
    }

    std::cout<<"Move-in B"<<"\n";
    // Move-in B
    for (int n = 0; n < block_n; n++) {
        for (int k = 0; k < block_k; k += block_per_row) {
            uint64_t dram_b_addr = dram_b_start + n * T_N * K + k * T_K;
            uint64_t sp_b_addr = sp_b_start + n * block_k * T_N + k * T_M;
            
            // std::cout<<"n="<<n<<" k="<<k<<std::endl;

            tile_t cols = block_per_row * T_K - ((k + block_per_row >= block_k) ? pad_K : 0);
            tile_t rows = T_N - ((n == block_n -1) ? pad_N : 0);

            // std::cout<<"Move B from DRAM "<<dram_b_addr<<" to SP "<<sp_b_addr
            //          <<" rows "<<rows<<" cols "<<cols<<std::endl;

            Command *move_b = new Command(idx++, LOAD, MOVE_IN);
            move_b->set_params(dram_b_addr, sp_b_addr, cols, rows);
            stonne_instance->get_sac()->receive(move_b);
        }
    }

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

                // uint64_t sp_a_addr = sp_a_start + m * T_M * K + k * T_K;
                // uint64_t sp_b_addr = sp_b_start + n * T_N * K + k * T_K;
                // uint64_t sp_c_addr = sp_c_start + m * T_M * N + n * T_N;
                
                uint64_t sp_a_addr = sp_a_start + m * block_k * T_M + k * T_M;
                uint64_t sp_b_addr = sp_b_start + n * block_k * T_N + k * T_N;
                uint64_t sp_c_addr = sp_c_start + m * block_n * T_M + n * T_M;

                // std::cout<<"sp A addr="<<sp_a_addr<<" sp B addr="<<sp_b_addr
                //          <<" sp C addr="<<sp_c_addr<<std::endl;

                // std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                mm->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN, sp_a_addr, sp_b_addr, sp_c_addr);
                mm->set_stay(true); // stay in SA

                 if(k == block_k -1) {
                    mm->set_stay(false); // last mm do not stay
                 }
                
                stonne_instance->get_sac()->receive(mm);
            }
        }
    }
    
    // Move-out C
    for(int m = 0; m < block_m; m++) {
        for(int n = 0; n < block_n; n++) {
            uint64_t dram_c_addr = dram_c_start + m * T_M * N + n * T_N;
            uint64_t sp_c_addr = sp_c_start + m * block_n * T_M + n * T_M;

            tile_t cols = T_N - ((n == block_n -1) ? pad_N : 0);
            tile_t rows = T_M - ((m == block_m -1) ? pad_M : 0);

            // std::cout<<"Move C from SP "<<sp_c_addr<<" to DRAM "<<dram_c_addr
            //          <<" rows "<<rows<<" cols "<<cols<<std::endl;

            Command *move_c = new Command(idx++, STORE, MOVE_OUT);
            move_c->set_params(dram_c_addr, sp_c_addr, cols, rows);
            stonne_instance->get_sac()->receive(move_c);
        }
    }



#endif

// only mm & irregular
#if 0
    int block_m = M / T_M + (M % T_M == 0 ? 0 : 1);
    int block_n = N / T_N + (N % T_N == 0 ? 0 : 1);
    int block_k = K / T_K + (K % T_K == 0 ? 0 : 1);

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

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

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                int valid_T_K = T_K;
                if(k == block_k -1 && (K % T_K != 0)) {
                    valid_T_K = K % T_K;
                }

                Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                mm->set_params(valid_T_N, valid_T_K, valid_T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                stonne_instance->get_sac()->receive(mm);
            }
        }
    }
#endif

// only mm
#if 0
    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;

    Command* conf = new Command(idx++, EXE, CONFIG);
    conf->set_params(T_N, T_K, T_M, (address_t)MK_matrix, (address_t)KN_matrix, (address_t)accbuf, N, K, M);
    stonne_instance->get_sac()->receive(conf);

    for (int m = 0; m < block_m; m++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_MN = m * T_M * N + n * T_N;

            for (int k = 0; k < block_k; k++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_KN = n * T_N * K + k * T_K;

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
                mm->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);

// dependeny between mm and previous mm
#if 0
                if(sac->get_inst_queue_size() > 0) {
                    Command* back = sac->get_inst_queue_back();
                    back->insert_child(mm);
                    mm->insert_parent(back);

                    std::cout<<"mm:"<<mm->get_seq_num()<< " depends "<<back->get_seq_num()<<std::endl;
                }
#endif
                
                stonne_instance->get_sac()->receive(mm);
            }
        }
    }
#endif

// all dependency between mm and pre
// this->output_address[offset_to_MN + current_M * N + i] = 0; in memory
#if 0
    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;
    for (int m = 0; m < block_m; m++) {
        for (int n = 0; n < block_n; n++) {
            int offset_to_MN = m * T_M * N + n * T_N;

            for (int k = 0; k < block_k; k++) {
                int offset_to_MK = m * T_M * K + k * T_K;
                int offset_to_KN = n * T_N * K + k * T_K;

                std::cout<<"MK="<<offset_to_MK<<" KN="<<offset_to_KN<<" MN="<<offset_to_MN<<std::endl;

                Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
                mm->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                if(sac->get_inst_queue_size() > 0) {
                    Command* back = sac->get_inst_queue_back();
                    back->insert_child(mm);
                    mm->insert_parent(back);

                    std::cout<<"mm:"<<mm->get_seq_num()<< " depends "<<back->get_seq_num()<<std::endl;
                }
                
                sac->receive(mm);

                if (k < block_k - 1) {
                    Command* pre = new Command(idx++, EXE, PRELOAD);

                    std::cout <<"idx="<<idx - 1 << " m=" << m << " n=" << n << " k=" << k
                              << " preload: " << offset_to_MN << std::endl;
                    pre->set_params(T_N, T_K, T_M, offset_to_MK, offset_to_MN, offset_to_KN);

                    mm->insert_child(pre);
                    pre->insert_parent(mm);

                    sac->receive(pre);
                    
                    std::cout<< "pre:"<<pre->get_seq_num()<< " depends "<<mm->get_seq_num()<<std::endl;
                }
            }
        }
    }
#endif

// fence
#if 0
    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;

    int offset_to_MN_1 = 0; 
    int offset_to_MK_1 = 0;
    int offset_to_KN_1 = 0;

    std::cout<<"MK="<<offset_to_MK_1<<" KN="<<offset_to_KN_1<<" MN="<<offset_to_MN_1<<std::endl;

    Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY);
    mm->set_params(T_N, T_K, T_M, offset_to_MK_1, offset_to_MN_1, offset_to_KN_1);
    sac->receive(mm);

    int offset_to_MN_2 = T_M * N;
    int offset_to_MK_2 = T_M * K;
    int offset_to_KN_2 = 0;

    Command* mm1 = new Command(idx++, EXE, MATRIX_MULTIPLY);
    mm1->set_params(T_N, T_K, T_M, offset_to_MK_2, offset_to_MN_2, offset_to_KN_2);
    sac->receive(mm1);

    Command* fence = new Command(idx++, EXE, FENCE);
    sac->receive(fence);

    Command* pre = new Command(idx++, EXE, PRELOAD);
    pre->set_params(T_N, T_K, T_M, offset_to_MK_1, offset_to_MN_1, offset_to_KN_2);
    sac->receive(pre);

    // pre->insert_parent(mm);
    // mm->insert_child(pre);

    int offset_to_MN_3 = 0;
    int offset_to_MK_3 = T_K;
    int offset_to_KN_3 = T_K;

    Command* mm2 = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
    mm2->set_params(T_N, T_K, T_M, offset_to_MK_3, offset_to_MN_3, offset_to_KN_3);

    sac->receive(mm2);

#endif

// overlap pre and previous mm
#if 0
    int block_m = M / T_M;
    int block_n = N / T_N;
    int block_k = K / T_K;

    int idx = 0;

    int offset_to_MN_1 = 0; 
    int offset_to_MK_1 = 0;
    int offset_to_KN_1 = 0;

    std::cout<<"MK="<<offset_to_MK_1<<" KN="<<offset_to_KN_1<<" MN="<<offset_to_MN_1<<std::endl;
    
    Command* pre = new Command(idx++, EXE, PRELOAD);
    pre->set_params(T_N, T_K, T_M, offset_to_MK_1, offset_to_MN_1, offset_to_KN_1);
    sac->receive(pre);

    Command* mm = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
    mm->set_params(T_N, T_K, T_M, offset_to_MK_1, offset_to_MN_1, offset_to_KN_1);
    sac->receive(mm);

    int offset_to_MN_2 = T_M * N;
    int offset_to_MK_2 = T_M * K;
    int offset_to_KN_2 = 0;

    Command* pre2 = new Command(idx++, EXE, PRELOAD);
    pre2->set_params(T_N, T_K, T_M, offset_to_MK_2, offset_to_MN_2, offset_to_KN_2);
    sac->receive(pre2);

    Command* mm1 = new Command(idx++, EXE, MATRIX_MULTIPLY_FLIP);
    mm1->set_params(T_N, T_K, T_M, offset_to_MK_2, offset_to_MN_2, offset_to_KN_2);
    sac->receive(mm1);

#endif

    stonne_instance->run(); // Running the simulator

#if 1

    //print accbuf
    std::cout << "Accbuf matrix: " << std::endl;
    for (int i = 0; i < M ; i++)
    {
        for (int j = 0; j < N ; j++)
        {   
            std::cout << accbuf[i * (N) + j] << " ";
        }   
        std::cout << std::endl;
    }

    // accbuf = unpad(accbuf, M + pad_M, N + pad_N, pad_M, pad_N);
    // Comparing the results
    for (int i = 0; i < output_size; i++)
    {
        float difference = fabs(accbuf[i] - output_cpu[i]);
        if (difference > EPSILON)
        {
            std::cout << "ERROR position " << i << ": Value ofmap simulator: " << accbuf[i] << ". Value ofmap CPU: " << output_cpu[i] << std::endl;
            std::cout << "\033[1;31mT test not passed\033[0m" << std::endl;
            delete[] MK_matrix;
            delete[] KN_matrix;
            delete[] output_cpu;
            delete[] accbuf;
            delete stonne_instance;
            assert(false); // Always false
        }
    }

    // If the code does not stop then the TEST is correct
    std::cout << "\033[1;32mTest passed correctly \033[0m" << std::endl
              << std::endl;
#endif

    delete[] MK_matrix;
    delete[] KN_matrix;
    delete[] output_cpu;
    delete[] accbuf;
    delete stonne_instance;
    return true;
}

// This function modifies the default values of the parameters according to user arguments.
void configDenseGEMMParameters(Config &stonne_cfg, NPUConfig *npuConfig)
{
    stonne_cfg.m_MSNetworkCfg.ms_rows = npuConfig->ms_rows;
    stonne_cfg.m_MSNetworkCfg.ms_cols = npuConfig->ms_cols;
    stonne_cfg.m_SDMemoryCfg.n_read_ports = npuConfig->dn_bw;
    stonne_cfg.m_SDMemoryCfg.n_write_ports = npuConfig->rn_bw;
    stonne_cfg.m_ASNetworkCfg.reduce_network_type = npuConfig->rn_type;
    stonne_cfg.m_MSNetworkCfg.multiplier_network_type = npuConfig->mn_type;
    stonne_cfg.m_SDMemoryCfg.mem_controller_type = npuConfig->mem_ctrl;
    stonne_cfg.m_ASNetworkCfg.accumulation_buffer_enabled = npuConfig->accumulation_buffer_enabled;
    stonne_cfg.dataflow = npuConfig->dataflow;
}
