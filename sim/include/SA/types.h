// Created 13/06/2019

#ifndef __types__h__

#define __types__h__

#include <cstdint>

#define word_size 1
#define IND_SIZE 4

typedef int data_t;
typedef int* address_t;
#define DATA_SIZE 1 // do not forget to change this if data_t changes
// typedef float data_t;
// typedef float* address_t;
// #define DATA_SIZE 4 // do not forget to change this if data_t changes

typedef unsigned int bandwidth_t;
typedef unsigned int id_t;
typedef unsigned int cycles_t;
typedef unsigned int counter_t;
typedef unsigned int latency_t;
typedef unsigned int* metadata_address_t;

typedef uint64_t CmdSeqNum;
typedef unsigned int tile_t;

enum operand_t { WEIGHT, IACTIVATION, OACTIVATION, PSUM , COMPUTED_PSUM};
enum traffic_t { BROADCAST, MULTICAST, UNICAST };
enum direction_t { LEFT, RIGHT };
// Adder configuration signals
enum fl_t { RECEIVE, SEND, NOT_CONFIGURED };  /// forwarding link type
enum adderconfig_t {
    ADD_2_1,
    ADD_3_1,
    ADD_1_1_PLUS_FW_1_1,
    FW_2_2,
    NO_MODE,
    FOLD
};  // To the best of my knowledge, FW_2_2 corresponds with sum left and right
    // and send the result to the FW.
/////
enum Layer_t { CONV, POOL, FC, GEMM, SPARSE_DENSE };
enum ReduceNetwork_t { ASNETWORK, FENETWORK, TEMPORALRN };
enum MultiplierNetwork_t { LINEAR, OS_MESH, WS_MESH };
/////
enum MemoryController_t {
    MAERI_DENSE_WORKLOAD,
    SIGMA_SPARSE_GEMM,
    MAGMA_SPARSE_DENSE,
    TPU_OS_DENSE
};
enum SparsityControllerState {
    CONFIGURING,
    DIST_STA_MATRIX,
    DIST_STR_MATRIX,
    WAITING_FOR_NEXT_STA_ITER,
    ALL_DATA_SENT
};
enum OSMeshControllerState {
    OS_CONFIGURING,
    OS_DIST_PSUMS,
    OS_DIST_INPUTS,
    OS_WAITING_FOR_NEXT_ITER,
    OS_ALL_DATA_SENT,
    OS_IDLE
};

enum WSMeshControllerState {
    WS_CONFIGURING,
    WS_DIST_WEIGHT,
    WS_DIST_INPUTS,
    WS_WAITING_FOR_NEXT_ITER,
    WS_ALL_DATA_SENT,
    WS_IDLE
};
/////
enum Dataflow {
    CNN_DATAFLOW,
    MK_STA_KN_STR,
    MK_STR_KN_STA,
    SPARSE_DENSE_DATAFLOW
};
enum GENERATION_TYPE { GEN_BY_ROWS, GEN_BY_COLS };
enum WIRE_TYPE { RN_WIRE, MN_WIRE, DN_WIRE };

enum adderoperation_t { ADDER, COMPARATOR, MULTIPLIER, NOP };

// Testbench
enum layerTest { TINY, LATE_SYNTHETIC, EARLY_SYNTHETIC, VGG_CONV11, VGG_CONV1 };

enum CommandType { LOAD, STORE, EXE, LOAD_ACC };
enum OperationType { PRELOAD, MATRIX_MULTIPLY, MATRIX_MULTIPLY_FLIP, FENCE, CONFIG, MOVE_IN, MOVE_OUT, MOVE_IN_ACC, FAKE, END };
enum DataflowType { DATAFLOW_WS, DATAFLOW_OS };
enum CommandStatus {
    COSIM_INIT,
    COSIM_WAITING,  // in queue
    COSIM_ISSUED,   // in controller
    COSIM_EXECUTING,
    COSIM_FINISHED,
    COSIM_COMMITTED
};

#endif
