#ifndef MANUAL_INTERFACE_H
#define MANUAL_INTERFACE_H

#include <stdint.h>
#include <limits.h>
#include <stdio.h>

// typedef int8_t data_t;
typedef float data_t;
// Host-side simulation passes pointers as "addresses".
// Use uintptr_t so casts from pointers are well-formed on 64-bit hosts.
//typedef uintptr_t address_t;
typedef uint64_t address_t;
typedef unsigned int tile_t;

#ifdef __cplusplus
extern "C" {
#endif

void cosim_config(tile_t T_N,
                  tile_t T_K,
                  tile_t T_M,
                  address_t address_MK,
                  address_t address_MN,
                  address_t address_KN,
                  tile_t N,
                  tile_t K,
                  tile_t M);

void cosim_preload(tile_t T_N,
                   tile_t T_K,
                   tile_t T_M,
                   address_t address_MK,
                   address_t address_MN,
                   address_t address_KN,
                address_t sp_address_MK,
                address_t sp_address_MN,
                address_t sp_address_KN);

void cosim_matrix_multiply(tile_t T_N,
                           tile_t T_K,
                           tile_t T_M,
                           address_t address_MK,
                           address_t address_MN,
                           address_t address_KN,
                           address_t sp_address_MK,
                           address_t sp_address_MN,
                           address_t sp_address_KN);

void cosim_matrix_multiply_flip(tile_t T_N,
                                tile_t T_K,
                                tile_t T_M,
                                address_t address_MK,
                                address_t address_MN,
                                address_t address_KN, 
                                address_t sp_address_MK,
                                address_t sp_address_MN,
                                address_t sp_address_KN);
    
void cosim_matrix_multiply_stay(tile_t T_N,
                                tile_t T_K,
                                tile_t T_M,
                                address_t address_MK,
                                address_t address_MN,
                                address_t address_KN,
                                address_t sp_address_MK,
                                address_t sp_address_MN,
                                address_t sp_address_KN);

void cosim_fence(void);

void cosim_mvin(address_t dram, address_t sp, tile_t cols, tile_t rows);
void cosim_mvout(address_t dram, address_t sp, tile_t cols, tile_t rows);
void cosim_mvin_acc(address_t dram, address_t sp, tile_t cols, tile_t rows);
void cosim_fake();

#ifdef __cplusplus
}
#endif

#endif