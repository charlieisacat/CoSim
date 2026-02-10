#ifndef INCLUDE_CORE_H
#define INCLUDE_CORE_H

#include "hardware.h"
#include "task/task.h"
#include "task/dyn_insn.h"
#include <stack>
#include "window.h"
#include "params.h"
#include "function_unit.h"
#include "port.h"
#include "issue_unit.h"
#include "lsu.h"
#include "mem/cache.h"
#include "mem/DRAM.hpp"
#include "branch_predictor.h"
#include "statistic.h"
#include "power_area_model.h"
#include "alloc_queue.h"

#include "ifu.h"

#include <vector>

#include "SA/STONNEModel.h"
#include "SA/types.h"
#include "SA/testbench.h"
#include <string>
#include <math.h>
#include "SA/utility.h"
#include <cstring>

static const int sp_addr_mask = 0x3FFFFFFF;

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

class Core : public Hardware
{
public:
    Core(std::shared_ptr<SIM::Task> _task, std::shared_ptr<Params> _params);

    virtual void run() override;

    /********************************* */
    // some statistics
    uint64_t flushN = 0;
    uint64_t funcCycles = 0;
    uint64_t flushedInsnNumber = 0;
    uint64_t rb_flushedN = 0;
    uint64_t stalls_mem_any = 0;
    
    uint64_t stalls_fetch = 0;
    uint64_t flush_stalls_fetch = 0;
    uint64_t ftq0_stalls_fetch = 0;
    uint64_t mispredict_stalls_fetch = 0;

    uint64_t temp_acc_cycle = 0;

    // just for check correctness
    uint64_t totalFetch = 0;
    uint64_t totalExe = 0;

    uint64_t globalDynID = 0;

    bool flushStall = false;
    int flushStallCycle = 0;
    int flushPenalty = 0;

    // backend bound cycles
    uint64_t stalls_total = 0;
    uint64_t cycles_ge_1_uop_exec = 0;
    uint64_t cycles_ge_2_uop_exec = 0;
    uint64_t cycles_ge_3_uop_exec = 0;

    uint64_t rs_empty_cycles = 0;
    uint64_t stall_sb = 0;
    uint64_t rob_full = 0;
    uint64_t iq_full = 0;
    uint64_t stall_load = 0;

    int int_phy_regs = 256;
    int fp_phy_regs = 256;

    /********************************* */

    virtual void initialize() override;
    virtual void finish() override;
    virtual void printStats() override;
    void printForMcpat();

    std::shared_ptr<L1Cache> getL1ICache() { return l1i; };
    std::shared_ptr<L1Cache> getL1Cache() { return l1c; };
    std::shared_ptr<L2Cache> getL2Cache() { return l2c; };
    std::shared_ptr<Window> getWindow() { return window; }

    uint64_t getTotalCommit();

    uint64_t print_total_cycles = 0;
    uint64_t print_mem_stall = 0;
    double print_cpi = 0;

private:

    uint64_t fast_forward_cycle = 0;
    uint64_t fast_forward_insn = 0;
    bool found = false;

    uint64_t get_cache_line_addr(uint64_t addr, int cache_line_size);
    std::queue<int> fetchCount;

    uint64_t last_fetch_cache_line = 0;

    // L1D Cache
    std::shared_ptr<L1Cache> l1c;
    // L1i Cache
    std::shared_ptr<L1Cache> l1i;
    // L2 Cache
    std::shared_ptr<L2Cache> l2c;

    std::shared_ptr<Window> window;

    // for BP 
    std::stack<std::shared_ptr<DynamicOperation>> prevBBTermStack;
    std::shared_ptr<DynamicOperation> prevBBTerminator;

    // FTP only stores starting insn of a basic block
    std::vector<std::shared_ptr<DynamicOperation>> FTQ;
    std::vector<std::shared_ptr<DynamicOperation>> insnQueue;
    std::vector<std::shared_ptr<DynamicOperation>> processedOps;
    std::vector<std::shared_ptr<DynamicOperation>> fetchBuffer;
    uint64_t insnQueueMaxSize = 1024;

    static const int fetchQueueSize = 16;
    std::vector<std::shared_ptr<DynamicOperation>> fetchQueue;

    // stack for return address
    std::stack<RetOps *> returnStack;
    // corresponding previous BB of return
    std::stack<std::shared_ptr<SIM::BasicBlock>> prevBBStack;

    // fetch
    std::shared_ptr<SIM::BasicBlock> previousBB = nullptr;
    std::shared_ptr<SIM::BasicBlock> targetBB = nullptr;
    std::vector<std::shared_ptr<DynamicOperation>> fetchOpBuffer;
    bool fetchForRet = false;
    void fetch();

    std::vector<std::shared_ptr<DynamicOperation>> preprocess();

    std::shared_ptr<DynamicOperation> lastOp = nullptr;

    // dispatch
    void dispatch();
    void insnToIssueQueues(std::vector<std::shared_ptr<DynamicOperation>> &insnToIssue);
    std::vector<std::shared_ptr<DynamicOperation>> issueQueue;
    // insn in fu pipelines
    std::vector<std::shared_ptr<DynamicOperation>> inFlightInsn;

    std::map<uint64_t, std::shared_ptr<DynamicOperation>> uidDynOpMap;
    std::map<uint64_t, int> dynIdTagMap;

    std::vector<std::shared_ptr<DynamicOperation>> cosimIssueQueue;

    // issue
    void issue();
    std::vector<std::shared_ptr<Port>> ports;
    std::shared_ptr<LSUnit> lsu;

    // decode
    void decode();
    std::vector<std::shared_ptr<DynamicOperation>> to_decode;

    // rename
    void rename();
    std::vector<std::shared_ptr<DynamicOperation>> to_rename;

    // execute
    void execute();
    void cosim_issue(DynInstPtr insn);

    // issue and execute
    void IE();

    // write back
    void writeBack();

    // commit
    std::vector<uint64_t> commitInsns;
    void commit();

    // flush
    std::vector<std::shared_ptr<DynamicOperation>> flushedInsns;

    // issue unit
    std::shared_ptr<IssueUnit> issueUnit;

    std::shared_ptr<Bpred> bp;

    bool abnormalExit = false;

    std::shared_ptr<IFU> ifu;

    std::shared_ptr<AllocQueue> allocQueue;

    uint64_t totalAllocQSize = 0;
    uint64_t totalAllocTime = 0;
    
    std::list<std::shared_ptr<DynamicOperation>> instList;
    void commitInsn(InstSeqNum seqNum);

    void readCosimArgs(std::shared_ptr<CallDynamicOperation> call);
    void readCosimMemArgs(std::shared_ptr<CallDynamicOperation> call);

    Stonne *stonne = nullptr;

    std::queue<data_t *> mk_ptrs;
    std::queue<data_t *> nk_ptrs;
    std::queue<data_t *> mn_ptrs;
    std::queue<data_t *> cpu_out_ptrs;
    
    std::queue<uint64_t> real_mk_ptrs;
    std::queue<uint64_t> real_kn_ptrs;
    std::queue<uint64_t> real_mn_ptrs;

    std::queue<std::pair<int, int>> mn_sizes;

    unsigned int MAX_RANDOM = 10; // Variable used to generate the random values

    uint64_t cosim_insn_num = 0;
    uint64_t cosim_mm_num = 0;
    uint64_t cosim_preload_num = 0;
};

#endif