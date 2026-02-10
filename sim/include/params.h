#ifndef INCLUDE_PARAMS_H
#define INCLUDE_PARAMS_H
#include <cstdint>
#include <map>
#include <string>
#include "task/llvm.h"
#include "function_unit.h"
#include "yaml-cpp/yaml.h"
#include "branch_predictor.h"

struct InstConfig
{
    int opcode_num;
    int runtime_cycles;
    int encap_n = 0;
    std::vector<std::pair<int, std::vector<int>>> ports;
    std::vector<std::vector<int>> fus;
};

struct Params
{
public:
    uint64_t windowSize = 1;
    uint64_t retireSize = 1;
    uint64_t robSize = 128;
    uint64_t issueQSize = 128;
    std::map<uint64_t, std::shared_ptr<InstConfig>> insnConfigs;
    std::map<int, std::vector<int>> portConfigs;
    std::vector<int> notPipelineFUs;
    bool simpleDram = false;
    int flushPenalty = 1;
    int clockSpeed = 2100;
    int ldqSize = 128;
    int stqSize = 128;
    int int_phy_regs = 256;
    int fp_phy_regs = 256;

    // l1 i-cache
    int l1i_latency = 1;
    int l1i_CLSize = 1;
    int l1i_size = 1;
    int l1i_assoc = 1;
    int l1i_mshr = 0;
    int l1i_store_ports = 8;
    int l1i_load_ports= 8;

    // l1cache
    int l1_latency = 1;
    int l1_CLSize = 1;
    int l1_size = 1;
    int l1_assoc = 1;
    int l1_mshr = 0;
    int l1_store_ports = 8;
    int l1_load_ports= 8;

    // l2cache
    int l2_latency = 1;
    int l2_CLSize = 1;
    int l2_size = 1;
    int l2_assoc = 1;
    int l2_mshr = 0;
    int l2_store_ports = 8;
    int l2_load_ports= 8;

    // l3cache
    int l3_latency = 1;
    int l3_CLSize = 1;
    int l3_size = 1;
    int l3_assoc = 1;
    int l3_mshr = 0;
    int l3_store_ports = 8;
    int l3_load_ports= 8;

    // dram
    int dram_latency = 100;
    int dram_bw = 10; // GB/sec

    std::string DRAM_system = "";
    std::string DRAM_device = "";

    bool naive = false;

    // bp
    int bhtSize = 4;
    TypeBpred bpType = bp_perfect;

    // number of bits for the global history register
    int ghrSize = 10;

    // dynamic fu count
    int threshold = -1;

    int lsu_port = 0;

    unsigned wayNum = 4;
    unsigned localPredictorSize = 4096;
    unsigned localCtrBits = 2;
    unsigned globalPredictorSize = 4096;
    unsigned globalCtrBits = 2;
    unsigned choicePredictorSize = 4096;
    unsigned choiceCtrBits = 2;
    unsigned uchLoadSize = 6;

    // ===== STONNE / NPU (cosim accelerator) =====
    // Controls whether STONNE DMA requests are injected into the core L2 cache.
    bool stonne_dma_to_l2 = false;

    // NPUConfig parameters (used to build STONNE Config)
    unsigned int npu_ms_rows = 16;
    unsigned int npu_ms_cols = 16;
    unsigned int npu_dn_bw = 32;
    unsigned int npu_rn_bw = 64;
    unsigned int npu_T_N = 16;
    unsigned int npu_T_M = 16;
    unsigned int npu_T_K = 16;
    unsigned int npu_dram_latency = 30;
    unsigned int npu_dma_bandwidth = 64;
    unsigned int npu_dma_max_in_flight = 16;

    // Stored as strings to keep Params independent of SA enum headers.
    std::string npu_rn_type = "TEMPORALRN";
    std::string npu_mn_type = "WS_MESH";
    std::string npu_mem_ctrl = "TPU_OS_DENSE";
    std::string npu_dataflow = "DATAFLOW_WS";

    unsigned int npu_M = 0;
    unsigned int npu_N = 0;
    unsigned int npu_K = 0;
    bool npu_accumulation_buffer_enabled = true;
};

#endif