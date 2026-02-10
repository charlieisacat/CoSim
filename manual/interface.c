#include "interface.h"

void __attribute__((used)) __attribute__((noinline)) cosim_config(tile_t T_N,
                  tile_t T_K,
                  tile_t T_M,
                  address_t address_MK,
                  address_t address_MN,
                  address_t address_KN,
                  tile_t N,
                  tile_t K,
                  tile_t M) {
    (void)N;
    (void)K;
    (void)M;
    (void)address_MK;
    (void)address_MN;
    (void)address_KN;
    (void)T_N;
    (void)T_K;
    (void)T_M;
}

void __attribute__((used)) __attribute__((noinline)) cosim_preload(tile_t T_N,
                   tile_t T_K,
                   tile_t T_M,
                   address_t address_MK,
                   address_t address_MN,
                   address_t address_KN, 
                address_t sp_address_MK,
                address_t sp_address_MN,
                address_t sp_address_KN) {
    (void)T_N;
    (void)T_K;
    (void)T_M;
    (void)address_MK;
    (void)address_MN;
    (void)address_KN;
}

void __attribute__((used)) __attribute__((noinline)) cosim_matrix_multiply(tile_t T_N,
                           tile_t T_K,
                           tile_t T_M,
                           address_t address_MK,
                           address_t address_MN,
                           address_t address_KN, 
                           address_t sp_address_MK,
                           address_t sp_address_MN,
                           address_t sp_address_KN) {
    (void)T_N;
    (void)T_K;
    (void)T_M;
    (void)address_MK;
    (void)address_MN;
    (void)address_KN;
}

void __attribute__((used)) __attribute__((noinline)) cosim_matrix_multiply_flip(tile_t T_N,
                                tile_t T_K,
                                tile_t T_M,
                                address_t address_MK,
                                address_t address_MN,
                                address_t address_KN,
                                address_t sp_address_MK,
                                address_t sp_address_MN,
                                address_t sp_address_KN) {
    (void)T_N;
    (void)T_K;
    (void)T_M;
    (void)address_MK;
    (void)address_MN;
    (void)address_KN;
}

void __attribute__((used)) __attribute__((noinline)) cosim_matrix_multiply_stay(tile_t T_N,
                                tile_t T_K,
                                tile_t T_M,
                                address_t address_MK,
                                address_t address_MN,
                                address_t address_KN,
                                address_t sp_address_MK,
                                address_t sp_address_MN,
                                address_t sp_address_KN) {
    (void)T_N;
    (void)T_K;
    (void)T_M;
    (void)address_MK;
    (void)address_MN;
    (void)address_KN;
}

void __attribute__((used)) __attribute__((noinline)) cosim_fence(void) {
}

void __attribute__((used)) __attribute__((noinline)) cosim_mvin(address_t dram, address_t sp, tile_t cols, tile_t rows) {
    (void)dram;
    (void)sp;
    (void)rows;
    (void)cols;
}

void __attribute__((used)) __attribute__((noinline)) cosim_mvout(address_t dram, address_t sp, tile_t cols, tile_t rows) {
    (void)dram;
    (void)sp;
    (void)rows;
    (void)cols;
}

void __attribute__((used)) __attribute__((noinline)) cosim_mvin_acc(address_t dram, address_t sp, tile_t cols, tile_t rows) {
    (void)dram;
    (void)sp;
    (void)rows;
    (void)cols;
}

void __attribute__((used)) __attribute__((noinline)) cosim_fake() {
}
