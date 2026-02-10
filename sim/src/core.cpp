#include "core.h"
#include "function_unit.h"

#include <stdio.h>
#include <iostream>

#include <chrono>
#include <fstream>

#include <math.h>
#include <string>
#include <cstring>
#include <cstdlib>

std::string insn_to_string_1(llvm::Value *v)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    rso << *v;        // This is where the instruction is serialized to the string.
    return rso.str(); // Return the resulting string.
}

Core::Core(std::shared_ptr<SIM::Task> _task, std::shared_ptr<Params> _params) : Hardware(_task, _params)
{
}

void Core::initialize()
{

    window = std::make_shared<Window>(params->windowSize, params->retireSize, params->robSize, params->issueQSize);
    issueUnit = std::make_shared<IssueUnit>(params);
    l1c = std::make_shared<L1Cache>(params->l1_latency, params->l1_CLSize, params->l1_size, params->l1_assoc, params->l1_mshr, params->l1_store_ports, params->l1_load_ports);
    l2c = std::make_shared<L2Cache>(params->l2_latency, params->l2_CLSize, params->l2_size, params->l2_assoc, params->l2_mshr, params->l2_store_ports, params->l2_load_ports);
    lsu = std::make_shared<LSUnit>(l1c, params->ldqSize, params->stqSize);

    l1i = std::make_shared<L1Cache>(params->l1i_latency, params->l1i_CLSize, params->l1i_size, params->l1i_assoc, params->l1i_mshr, params->l1i_store_ports, params->l1i_load_ports, true);
    ifu = make_shared<IFU>(l1i);

    lsu->lsu_port = params->lsu_port;
    fp_phy_regs = params->fp_phy_regs;
    int_phy_regs = params->int_phy_regs;

    // make sure enough insns are in insn queue
    insnQueueMaxSize = 64;

    issueUnit->naive = params->naive;

    // lsu and l1cache
    l1c->lsu = lsu;
    l1c->nextCache = l2c;

    l1i->ifu = ifu;
    l1i->nextCache = l2c;

    l2c->prevDCache = l1c;
    l2c->prevICache = l1i;

    for (auto item : params->portConfigs)
    {
        int portID = item.first;
        // std::cout << "portID=" << portID << "\n";
        ports.push_back(std::make_shared<Port>(portID, item.second));
    }

    flushPenalty = params->flushPenalty;
    bp = std::make_shared<Bpred>(params->bpType, params->bhtSize, params->ghrSize);
    stat = std::make_shared<Statistic>();

    pam = new PowerAreaModel();

    targetBB = fetchEntryBB();

    allocQueue = std::make_shared<AllocQueue>(140, window->windowSize);

    auto parse_dataflow_type = [](const std::string& s) {
        std::string x = to_lower(s);
        if (x == "dataflow_os" || x == "os")
            return DATAFLOW_OS;
        return DATAFLOW_WS;
    };

    ReduceNetwork_t rn_type = get_type_reduce_network_type(params->npu_rn_type);
    MultiplierNetwork_t mn_type = get_type_multiplier_network_type(params->npu_mn_type);
    MemoryController_t mem_ctrl = get_type_memory_controller_type(params->npu_mem_ctrl);
    DataflowType df = parse_dataflow_type(params->npu_dataflow);

    NPUConfig* npuConfig = new NPUConfig(
        params->npu_ms_rows,
        params->npu_ms_cols,
        params->npu_dn_bw,
        params->npu_rn_bw,
        params->npu_T_N,
        params->npu_T_M,
        params->npu_T_K,
        rn_type,
        mn_type,
        mem_ctrl,
        params->npu_M,
        params->npu_N,
        params->npu_K,
        params->npu_accumulation_buffer_enabled,
        df);
    Config stonne_cfg;

    stonne_cfg.m_MSNetworkCfg.ms_rows = npuConfig->ms_rows;
    stonne_cfg.m_MSNetworkCfg.ms_cols = npuConfig->ms_cols;
    stonne_cfg.m_SDMemoryCfg.n_read_ports = npuConfig->dn_bw;
    stonne_cfg.m_SDMemoryCfg.n_write_ports = npuConfig->rn_bw;
    stonne_cfg.m_ASNetworkCfg.reduce_network_type = npuConfig->rn_type;
    stonne_cfg.m_MSNetworkCfg.multiplier_network_type = npuConfig->mn_type;
    stonne_cfg.m_SDMemoryCfg.mem_controller_type = npuConfig->mem_ctrl;
    stonne_cfg.m_ASNetworkCfg.accumulation_buffer_enabled = npuConfig->accumulation_buffer_enabled;
    stonne_cfg.dataflow = npuConfig->dataflow;
    stonne_cfg.print_stats_enabled = false;

    stonne = new Stonne(stonne_cfg); // Creating instance of the simulator

    // Configure DMAEngine latency model to match DRAM latency (used in fallback mode).
    stonne->get_sac()->get_dma_engine()->set_base_latency(params->npu_dram_latency);
    stonne->get_sac()->get_dma_engine()->set_bandwidth(params->npu_dma_bandwidth);
    stonne->get_sac()->get_dma_engine()->set_max_in_flight_mem_reqs(params->npu_dma_max_in_flight);

    if (params->stonne_dma_to_l2)
        stonne->get_sac()->connect_dma_to_l2(l2c);
}

void Core::finish()
{
}

void Core::printForMcpat()
{
    std::ofstream out_file("mcpat.txt");
    uint64_t total_instructions = window->getTotalCommit();

    const std::set<uint64_t> int_opcodes = {13, 15, 34, 53, 48, 47, 25, 26, 27, 28, 29, 30, 17, 19, 20, 23, 22};
    const std::set<uint64_t> fp_opcodes = {18, 21, 24, 14, 16, 54, 46, 42, 41, 45, 43};
    uint64_t int_instructions = 0;
    uint64_t fp_instructions = 0;
    uint64_t branch_instructions = 0;
    uint64_t load_instructions = 0;
    uint64_t store_instructions = 0;
    uint64_t function_calls = 0;

    for (auto item : stat->insnCountMap)
    {
        llvm::Instruction *insn = item.first;
        uint64_t count = item.second;

        uint32_t opcode = insn->getOpcode();
        if (int_opcodes.count(opcode))
        {
            int_instructions += count;
        }
        else if (fp_opcodes.count(opcode))
        {
            fp_instructions += count;
        }
        else if (opcode == 2 || opcode == 3)
        {
            branch_instructions += count;
        }
        else if (opcode == 32)
        {
            load_instructions += count;
        }
        else if (opcode == 33)
        {
            store_instructions += count;
        }
        else if (opcode == 56 || opcode == 5)
        {
            function_calls += count;
        }
    }

    uint64_t branch_misprediction = window->mispredictNum;

    out_file << "total_instructions=" << total_instructions << "\n";
    out_file << "int_instructions=" << int_instructions << "\n";
    out_file << "fp_instructions=" << fp_instructions << "\n";
    out_file << "branch_instructions=" << branch_instructions << "\n";
    out_file << "branch_misprediction=" << branch_misprediction << "\n";
    out_file << "load_instructions=" << load_instructions << "\n";
    out_file << "store_instructions=" << store_instructions << "\n";
    out_file << "committed_instructions=" << total_instructions << "\n";
    out_file << "committed_int_instructions=" << int_instructions << "\n";
    out_file << "committed_fp_instructions=" << fp_instructions << "\n";

    uint64_t total_cycles = cycles;

    out_file << "committed_fp_instructions=" << fp_instructions << "\n";

    uint64_t ROB_reads = total_instructions;
    uint64_t ROB_writes = total_instructions;
    out_file << "ROB_reads=" << ROB_reads << "\n";
    out_file << "ROB_writes=" << ROB_writes << "\n";

    uint64_t inst_window_reads = total_instructions - fp_instructions;
    uint64_t inst_window_writes = total_instructions - fp_instructions;
    uint64_t inst_window_wakeup_accesses = total_instructions - fp_instructions;
    uint64_t fp_inst_window_reads = fp_instructions;
    uint64_t fp_inst_window_writes = fp_instructions;
    uint64_t fp_inst_window_wakeup_accesses = fp_instructions;
    out_file << "inst_window_reads=" << inst_window_reads << "\n";
    out_file << "inst_window_writes=" << inst_window_writes << "\n";
    out_file << "inst_window_wakeup_accesses=" << inst_window_wakeup_accesses << "\n";
    out_file << "fp_inst_window_reads=" << fp_inst_window_reads << "\n";
    out_file << "fp_inst_window_writes=" << fp_inst_window_writes << "\n";
    out_file << "fp_inst_window_wakeup_accesses=" << fp_inst_window_wakeup_accesses << "\n";

    uint64_t int_regfile_reads = stat->intRFReads;
    uint64_t int_regfile_writes = stat->intRFWrites;
    uint64_t float_regfile_reads = stat->fpRFReads;
    uint64_t float_regfile_writes = stat->fpRFWrites;

    out_file << "int_regfile_reads=" << int_regfile_reads << "\n";
    out_file << "int_regfile_writes=" << int_regfile_writes << "\n";
    out_file << "float_regfile_reads=" << float_regfile_reads << "\n";
    out_file << "float_regfile_writes=" << float_regfile_writes << "\n";

    out_file << "function_calls=" << function_calls << "\n";

    const std::set<uint64_t> ialu_opcodes = {13, 15, 34, 53, 48, 47, 25, 26, 27, 28, 29, 30};
    const std::set<uint64_t> int_mult_opcodes = {17, 19, 20, 23, 22};

    uint64_t fpu_accesses = fp_instructions;
    uint64_t ialu_accesses = 0;
    uint64_t mul_accesses = 0;

    for (auto item : stat->insnCountMap)
    {
        llvm::Instruction *insn = item.first;
        uint64_t count = item.second;

        uint32_t opcode = insn->getOpcode();
        if (ialu_opcodes.count(opcode))
        {
            ialu_accesses += count;
        }
        else if (int_mult_opcodes.count(opcode))
        {
            mul_accesses += count;
        }
    }

    out_file << "ialu_accesses=" << ialu_accesses << "\n";
    out_file << "fpu_accesses=" << fpu_accesses << "\n";
    out_file << "mul_accesses=" << mul_accesses << "\n";

    uint64_t l1i_read_accesses = l1i->read_accesses;
    uint64_t l1i_write_accesses = l1i->write_accesses;
    uint64_t l1i_read_misses = l1i->read_misses;
    uint64_t l1i_write_misses = l1i->write_misses;

    uint64_t l1_read_accesses = l1c->read_accesses;
    uint64_t l1_write_accesses = l1c->write_accesses;
    uint64_t l1_read_misses = l1c->read_misses;
    uint64_t l1_write_misses = l1c->write_misses;

    uint64_t l2_read_accesses = l2c->read_accesses;
    uint64_t l2_write_accesses = l2c->write_accesses;
    uint64_t l2_read_misses = l2c->read_misses;
    uint64_t l2_write_misses = l2c->write_misses;

    // uint64_t l3_read_accesses = l2c->nextCache->read_accesses;
    // uint64_t l3_write_accesses = l2c->nextCache->write_accesses;
    // uint64_t l3_read_misses = l2c->nextCache->read_misses;
    // uint64_t l3_write_misses = l2c->nextCache->write_misses;

    out_file << "l1_read_accesses=" << l1_read_accesses << "\n";
    out_file << "l1_write_accesses=" << l1_write_accesses << "\n";
    out_file << "l1_read_misses=" << l1_read_misses << "\n";
    out_file << "l1_write_misses=" << l1_write_misses << "\n";

    out_file << "l2_read_accesses=" << l2_read_accesses << "\n";
    out_file << "l2_write_accesses=" << l2_write_accesses << "\n";
    out_file << "l2_read_misses=" << l2_read_misses << "\n";
    out_file << "l2_write_misses=" << l2_write_misses << "\n";

    // out_file << "l3_read_accesses=" << l3_read_accesses << "\n";
    // out_file << "l3_write_accesses=" << l3_write_accesses << "\n";
    // out_file << "l3_read_misses=" << l3_read_misses << "\n";
    // out_file << "l3_write_misses=" << l3_write_misses << "\n";

    std::cout << "l1_read_accesses=" << l1_read_accesses << "\n";
    std::cout << "l1_write_accesses=" << l1_write_accesses << "\n";
    std::cout << "l1_read_misses=" << l1_read_misses << "\n";
    std::cout << "l1_write_misses=" << l1_write_misses << "\n";

    std::cout << "l2_read_accesses=" << l2_read_accesses << "\n";
    std::cout << "l2_write_accesses=" << l2_write_accesses << "\n";
    std::cout << "l2_read_misses=" << l2_read_misses << "\n";
    std::cout << "l2_write_misses=" << l2_write_misses << "\n";

    // std::cout << "l3_read_accesses=" << l3_read_accesses << "\n";
    // std::cout << "l3_write_accesses=" << l3_write_accesses << "\n";
    // std::cout << "l3_read_misses=" << l3_read_misses << "\n";
    // std::cout << "l3_write_misses=" << l3_write_misses << "\n";

    std::cout << "l1i_access=" << l1i_read_accesses + l1i_write_accesses << "\n";
    std::cout << "l1i_miss=" << l1i_read_misses + l1i_write_misses << "\n";

    std::cout << "l1_access=" << l1_read_accesses + l1_write_accesses << "\n";
    std::cout << "l1_miss=" << l1_read_misses + l1_write_misses << "\n";

    std::cout << "l2_access=" << l2_read_accesses + l2_write_accesses << "\n";
    std::cout << "l2_miss=" << l2_read_misses + l2_write_misses << "\n";

    // std::cout << "l3_access=" << l3_read_accesses + l3_write_accesses << "\n";
    // std::cout << "l3_miss=" << l3_read_misses + l3_write_misses << "\n";

    uint64_t l1i_access = l1i_read_accesses + l1i_write_accesses;
    uint64_t l1d_access = l1_read_accesses + l1_write_accesses;
    uint64_t l2_access = l2_read_accesses + l2_write_accesses;
    // uint64_t l3_access = l3_read_accesses + l3_write_accesses;

    uint64_t l1i_miss = l1i_read_misses + l1i_write_misses;
    uint64_t l1d_miss = l1_read_misses + l1_write_misses;
    uint64_t l2_miss = l2_read_misses + l2_write_misses;
    // uint64_t l3_miss = l3_read_misses + l3_write_misses;

    // std::cout << print_total_cycles << ", " << print_mem_stall << ", " << print_cpi << ", " << l1d_miss * 1.0 / l1d_access << ", " << l1i_miss << ", " << l2_miss * 1.0 / l2_access << ", " << l3_miss * 1.0 / l3_access << "\n";

    double custom_power = 0.;
    double custom_area = 0.;
    uint64_t total_custom_insn_num = 0;
    for (auto item : stat->customInsnCountMap)
    {
        uint32_t opcode = item.first;
        uint64_t count = item.second;

        double power = stat->customInsnPowerMap[opcode];
        double area = stat->customInsnAreaMap[opcode];
        custom_power += count * power;
        custom_area += area;
        total_custom_insn_num += count;
    }

    out_file << "total_custom_insn_num=" << total_custom_insn_num << "\n";
    out_file << "Custom_power=" << custom_power << "\n";
    out_file << "Custom_area=" << custom_area << "\n";

    out_file.close();
}

void Core::printStats()
{
    std::ofstream out_file("stat.txt");
    cycles -= fast_forward_cycle;

    std::cout << "totalFetch=" << totalFetch << "\n";
    uint64_t totalCommit = window->getTotalCommit() - fast_forward_insn;
    // uint64_t totalCommit = window->getTotalCommit();
    std::cout << "totalCommit=" << totalCommit << "\n";
    std::cout << "totalCycle=" << cycles << "\n";
    std::cout << "rob_full=" << rob_full << "\n";
    std::cout << "stall_load=" << stall_load << "\n";
    std::cout << "iq_full=" << iq_full << "\n";
    std::cout << "ldq_full=" << lsu->ldq_full << "\n";
    std::cout << "stq_full=" << lsu->stq_full << "\n";

    print_total_cycles = cycles;

    // top level
    uint64_t slots = cycles * window->windowSize;
    double retiring = totalCommit * 1.0 / slots;

    std::cout << "memordering=" << window->rb_flush << " mispred=" << window->mispredictNum << "\n";
    uint64_t recoverBubbles = (window->rb_flush + window->mispredictNum) * flushPenalty;
    double bad_speculation = (recoverBubbles * window->windowSize) * 1.0 / slots;

    std::cout << "==== top-down ====\n";
    std::cout << "retiring=" << retiring << "\n";
    std::cout << "bad_speculation=" << bad_speculation << "\n";
    std::cout << "backend=" << 1 - retiring - bad_speculation << "\n";

    const std::set<uint64_t> int_opcodes = {13, 15, 34, 53, 48, 47, 25, 26, 27, 28, 29, 30, 17, 19, 20, 23, 22};
    const std::set<uint64_t> fp_opcodes = {18, 21, 24, 14, 16, 54, 46, 42, 41, 45, 43};
    uint64_t int_instructions = 0;
    uint64_t fp_instructions = 0;
    uint64_t branch_instructions = 0;
    uint64_t load_instructions = 0;
    uint64_t store_instructions = 0;
    uint64_t function_calls = 0;

    for (auto item : stat->insnCountMap)
    {
        llvm::Instruction *insn = item.first;
        uint64_t count = item.second;

        uint32_t opcode = insn->getOpcode();
        if (int_opcodes.count(opcode))
        {
            int_instructions += count;
        }
        else if (fp_opcodes.count(opcode))
        {
            fp_instructions += count;
        }
        else if (opcode == 2 || opcode == 3)
        {
            branch_instructions += count;
        }
        else if (opcode == 32)
        {
            load_instructions += count;
        }
        else if (opcode == 33)
        {
            store_instructions += count;
        }
        else if (opcode == 56 || opcode == 5)
        {
            function_calls += count;
        }
    }

    uint64_t branch_misprediction = window->mispredictNum;

    std::cout << "int_instructions=" << int_instructions << "\n";
    std::cout << "fp_instructions=" << fp_instructions << "\n";
    std::cout << "branch_instructions=" << branch_instructions << "\n";
    std::cout << "branch_misprediction=" << branch_misprediction << "\n";
    std::cout << "load_instructions=" << load_instructions << "\n";
    std::cout << "store_instructions=" << store_instructions << "\n";
    std::cout << "committed_int_instructions=" << int_instructions << "\n";
    std::cout << "committed_fp_instructions=" << fp_instructions << "\n";
    std::cout <<" cosim insn_num="<<cosim_insn_num<<"\n";
    std::cout <<" cosim mm_num="<<cosim_mm_num<<"\n";
    std::cout <<" cosim preload_num="<<cosim_preload_num<<"\n";

    double ipc = totalCommit * 1.0 / cycles;
    uint64_t few_threshold = ipc > 1.8 ? cycles_ge_3_uop_exec : cycles_ge_2_uop_exec;

    print_cpi = cycles * 1.0 / totalCommit;
    print_mem_stall = stalls_mem_any;

    // we have no store buffer, so stall_sb is always 0
    uint64_t backend_bound_cycles = stalls_total + cycles_ge_1_uop_exec - few_threshold - rs_empty_cycles + stall_sb;

    std::cout << "ipc = " << ipc << "\n";
    std::cout << "cycles_ge_1_uop_exec = " << cycles_ge_1_uop_exec << "\n";
    std::cout << "cycles_ge_2_uop_exec = " << cycles_ge_2_uop_exec << "\n";
    std::cout << "cycles_ge_3_uop_exec = " << cycles_ge_3_uop_exec << "\n";
    std::cout << "few_threshold = " << few_threshold << "\n";
    std::cout << "stalls_total = " << stalls_total << "\n";
    std::cout << "stall_sb = " << stall_sb << "\n";
    std::cout << "rs_empty_cycles = " << rs_empty_cycles << "\n";
    std::cout << "backend_bound = " << backend_bound_cycles << "\n";
    std::cout << "stalls_mem_any = " << stalls_mem_any << "\n";
    std::cout << "stalls_fetch = " << stalls_fetch << "\n";
    std::cout << "ftq0_stalls_fetch = " << ftq0_stalls_fetch << "\n";
    std::cout << "flush_stalls_fetch = " << flush_stalls_fetch << "\n";
    std::cout << "mispredict_stalls_fetch = " << mispredict_stalls_fetch << "\n";
    std::cout << "flushN=" << window->rb_flush << " flushedINsn=" << flushedInsnNumber << "\n";

    out_file << "totalFetch=" << totalFetch << "\n";
    out_file << "totalCommit=" << totalCommit << "\n";
    out_file << "totalCycle=" << cycles << "\n";
    out_file << "rob_full=" << rob_full << "\n";
    out_file << "stall_load=" << stall_load << "\n";
    out_file << "iq_full=" << iq_full << "\n";
    out_file << "ldq_full=" << lsu->ldq_full << "\n";
    out_file << "stq_full=" << lsu->stq_full << "\n";
    out_file << "retiring=" << retiring << "\n";
    out_file << "bad_speculation=" << bad_speculation << "\n";
    out_file << "backend=" << 1 - retiring - bad_speculation << "\n";
    out_file << "int_instructions=" << int_instructions << "\n";
    out_file << "fp_instructions=" << fp_instructions << "\n";
    out_file << "branch_instructions=" << branch_instructions << "\n";
    out_file << "branch_misprediction=" << branch_misprediction << "\n";
    out_file << "load_instructions=" << load_instructions << "\n";
    out_file << "store_instructions=" << store_instructions << "\n";
    out_file << "committed_int_instructions=" << int_instructions << "\n";
    out_file << "committed_fp_instructions=" << fp_instructions << "\n";
    out_file << "cosim_insn_num=" << cosim_insn_num << "\n";
    out_file <<"cosim_mm_num="<<cosim_mm_num<<"\n";
    out_file <<"cosim_preload_num="<<cosim_preload_num<<"\n";

    out_file << "ipc=" << ipc << "\n";
    out_file << "cycles_ge_1_uop_exec=" << cycles_ge_1_uop_exec << "\n";
    out_file << "cycles_ge_2_uop_exec=" << cycles_ge_2_uop_exec << "\n";
    out_file << "cycles_ge_3_uop_exec=" << cycles_ge_3_uop_exec << "\n";
    out_file << "few_threshold=" << few_threshold << "\n";
    out_file << "stalls_total=" << stalls_total << "\n";
    out_file << "stall_sb=" << stall_sb << "\n";
    out_file << "rs_empty_cycles=" << rs_empty_cycles << "\n";
    out_file << "backend_bound=" << backend_bound_cycles << "\n";
    out_file << "stalls_mem_any=" << stalls_mem_any << "\n";
    out_file << "stalls_fetch=" << stalls_fetch << "\n";
    out_file << "ftq0_stalls_fetch=" << ftq0_stalls_fetch << "\n";
    out_file << "flush_stalls_fetch=" << flush_stalls_fetch << "\n";
    out_file << "mispredict_stalls_fetch=" << mispredict_stalls_fetch << "\n";
    out_file << "flushN=" << window->rb_flush << "\n";
    out_file << "flushedInsn=" << flushedInsnNumber << "\n";

    double mem_bound = (stalls_mem_any + stall_sb) * 1.0 / backend_bound_cycles;
    double core_bound = 1.0 - mem_bound;
    double backend = 1 - retiring - bad_speculation;
    std::cout << "======== " << 0. << ", " << bad_speculation << ", " << backend << " ," << retiring << ", " << mem_bound << ", " << core_bound << "\n";

    std::cout << "mem_bound=" << mem_bound << "\n";
    std::cout << "core_bound=" << core_bound << "\n";
    std::cout << "l1 DCache hit=" << l1c->getHitRate() << "\n";
    std::cout << "l1 ICache hit=" << l1i->getHitRate() << "\n";
    std::cout << "l2 hit=" << l2c->getHitRate() << "\n";
    // std::cout << "l3 hit=" << llc->getHitRate() << "\n";
    std::cout << "l1 port stall=" << l1c->portStall << "\n";
    std::cout << "l2 port stall=" << l2c->portStall << "\n";
    // std::cout << "l3 port stall=" << llc->portStall << "\n";
    std::cout << "bp mispredct num=" << window->mispredictNum << " total=" << window->totalPredictNum << "\n";
    std::cout << "BP Acc=" << 1. - (window->mispredictNum * 1.0 / window->totalPredictNum) << "\n";

    out_file << "mem_bound=" << mem_bound << "\n";
    out_file << "core_bound=" << core_bound << "\n";
    out_file << "l1_hit=" << l1c->getHitRate() << "\n";
    out_file << "l1i_hit=" << l1i->getHitRate() << "\n";
    out_file << "l2_hit=" << l2c->getHitRate() << "\n";
    // out_file << "l3_hit=" << llc->getHitRate() << "\n";
    out_file << "l1_port_stall=" << l1c->portStall << "\n";
    out_file << "l2_port_stall=" << l2c->portStall << "\n";
    // out_file << "l3_port_stall=" << llc->portStall << "\n";
    out_file << "BP_Acc=" << 1. - (window->mispredictNum * 1.0 / window->totalPredictNum) << "\n";
    out_file << "bp_mispredict_num=" << window->mispredictNum << "\n";
    out_file << "bp_total_num=" << window->totalPredictNum << "\n";
    out_file << "static_insn_size=" << stat->insnCountMap.size() << "\n";

    std::cout << "static insn szie=" << stat->insnCountMap.size() << "\n";

    const std::set<uint64_t> int_adder_opcodes = {13, 15, 34, 53, 48, 47};
    const std::set<uint64_t> bitwise_opcodes = {25, 26, 27, 28, 29, 30};
    const std::set<uint64_t> int_mult_opcodes = {17, 19, 20, 23, 22};
    const std::set<uint64_t> fp_mult_opcodes = {18, 21, 24};
    const std::set<uint64_t> fp_adder_opcodes = {14, 16, 54, 46, 42, 41, 45, 43};

    double int_adder_power = 0.;
    double int_mult_power = 0.;
    double fp_adder_power = 0.;
    double fp_mult_power = 0.;
    double bitwise_power = 0.;
    double reg_power = stat->totalWrite * 32 * (pam->fus[1]->internal_power + pam->fus[1]->switch_power);

    double int_adder_area = pam->fus[7]->fu_limit * pam->fus[7]->area;
    double int_mult_area = pam->fus[6]->fu_limit * pam->fus[6]->area;
    double fp_adder_area = pam->fus[3]->fu_limit * pam->fus[3]->area;
    double fp_mult_area = pam->fus[0]->fu_limit * pam->fus[0]->area;
    double bitwise_area = pam->fus[2]->fu_limit * pam->fus[2]->area;
    double reg_area = pam->fus[1]->fu_limit * pam->fus[1]->area * 32;

    for (auto item : stat->insnCountMap)
    {
        llvm::Instruction *insn = item.first;
        uint64_t count = item.second;

        uint32_t opcode = insn->getOpcode();
        HWFunctionalUnit *hwfu = pam->getHWFuncUnit(opcode);
        if (hwfu)
        {
            if (int_adder_opcodes.count(opcode))
            {
                int_adder_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (int_mult_opcodes.count(opcode))
            {
                int_mult_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (fp_adder_opcodes.count(opcode))
            {
                fp_adder_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (fp_mult_opcodes.count(opcode))
            {
                fp_mult_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (bitwise_opcodes.count(opcode))
            {
                bitwise_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
        }
        else
        {
            // std::cout << "not concern, opcode=" << item.first->getOpcode() << " opname=" << item.first->getOpcodeName() << "\n";
        }
    }

    double custom_power = 0.;
    double custom_area = 0.;
    uint64_t total_custom_insn_num = 0;
    for (auto item : stat->customInsnCountMap)
    {
        uint32_t opcode = item.first;
        uint64_t count = item.second;

        double power = stat->customInsnPowerMap[opcode];
        double area = stat->customInsnAreaMap[opcode];
        custom_power += count * power;
        custom_area += area;
        total_custom_insn_num += count;
        out_file << to_string(opcode) + "=" << count << "\n";
    }

    out_file << "slots=" << slots << "\n";
    out_file << "stalls_mem_any=" << stalls_mem_any << "\n";
    out_file << "total_custom_insn_num=" << total_custom_insn_num << "\n";
    out_file << "packing_func_num=" << stat->packing_func_num << "\n";
    out_file << "extract_insn_num=" << stat->extract_insn_num << "\n";

    std::cout << "total_custom_insn_num=" << total_custom_insn_num << "\n";
    std::cout << "packing_func_num=" << stat->packing_func_num << "\n";
    std::cout << "extract_insn_num=" << stat->extract_insn_num << "\n";
    std::cout << "total_new_insn_num=" << total_custom_insn_num + stat->packing_func_num + stat->extract_insn_num << "\n";

    std::cout << "Custom power=" << custom_power << " area=" << custom_area << "\n";
    std::cout << "Int adder power=" << int_adder_power << " area=" << int_adder_area << "\n";
    std::cout << "Int mult power=" << int_mult_power << " area=" << int_mult_area << "\n";
    std::cout << "Fp adder power=" << fp_adder_power << " area=" << fp_adder_area << "\n";
    std::cout << "Fp mult power=" << fp_mult_power << " area=" << fp_mult_area << "\n";
    std::cout << "Bitwise power=" << bitwise_power << " area=" << bitwise_area << "\n";
    std::cout << "Reg power=" << reg_power << " area=" << reg_area << "\n";

    std::cout<<"=== SA exe cycle=" <<stonne->get_sac()->exe_cycle<<"\n";
    std::cout<<"=== SA load cycle=" <<stonne->get_sac()->ld_cycle<<"\n";
    std::cout<<"=== SA store cycle=" <<stonne->get_sac()->st_cycle<<"\n";
    std::cout<<"=== SA load+store cycle=" <<stonne->get_sac()->ld_st_cycle<<"\n";


    double total_power = int_adder_power + int_mult_power + fp_adder_power + fp_mult_power + bitwise_power + reg_power + custom_power;
    double total_area = int_adder_area + int_mult_area + fp_adder_area + fp_mult_area + bitwise_area + reg_area + custom_area;
    std::cout << "total power=" << total_power << " area=" << total_area << "\n";

    std::cout << "stalls_custom_insn=" << stat->cstm_insn_stall << "\n";
    out_file << "stalls_custom_insn=" << stat->cstm_insn_stall << "\n";
    out_file << "packing_insn_stall=" << stat->packing_insn_stall << "\n";
    out_file << "ext_insn_stall=" << stat->ext_insn_stall << "\n";

    std::cout << "stalls_custom_insn_ratio=" << stat->cstm_insn_stall * 1.0 / cycles << "\n";
    std::cout << "alloca queue average size=" << totalAllocQSize * 1.0 / totalAllocTime << "\n";

    out_file << "stalls_custom_insn_ratio=" << stat->cstm_insn_stall * 1.0 / cycles << "\n";
    out_file << "packing_insn_stall_ratio=" << stat->packing_insn_stall * 1.0 / cycles << "\n";
    out_file << "ext_insn_stall_ratio=" << stat->ext_insn_stall * 1.0 / cycles << "\n";

    out_file << "totalFetch=" << totalFetch << "\n";
    out_file << "totalCommit=" << window->getTotalCommit() << "\n";
    out_file << "totalCycle=" << cycles << "\n";

    out_file << "Custom_power=" << custom_power << "\n";
    out_file << "Custom_area=" << custom_area << "\n";
    out_file << "Int_adder_power=" << int_adder_power << "\n";
    out_file << "Int_adder_area=" << int_adder_area << "\n";
    out_file << "Int_mult_power=" << int_mult_power << "\n";
    out_file << "Int_mult_area=" << int_mult_area << "\n";
    out_file << "Fp_adder_power=" << fp_adder_power << "\n";
    out_file << "Fp_adder_area=" << fp_adder_area << "\n";
    out_file << "Fp_mult_power=" << fp_mult_power << "\n";
    out_file << "Fp_mult_area=" << fp_mult_area << "\n";
    out_file << "Bitwise_power=" << bitwise_power << "\n";
    out_file << "Bitwise_area=" << bitwise_area << "\n";
    out_file << "Reg_power=" << reg_power << "\n";
    out_file << "Reg_area=" << reg_area << "\n";
    out_file << "Total_power=" << total_power << "\n";
    out_file << "Total_area=" << total_area << "\n";
    out_file.close();
}

void Core::decode()
{
    if (flushStall)
        return;

    if(to_rename.size())
        return;

    int decodeWidth = window->retireSize;
    for (int j = 0; j < decodeWidth; j++)
    {
        // if(allocQueue->isFull())
        //     break;
        if(to_rename.size() >= decodeWidth)
            break;

        if (to_decode.size() == 0)
            break;

        unsigned predDist = 0;

        auto inst = to_decode[0];
        // allocQueue->insert(inst, predDist);
        to_rename.push_back(inst);
        to_decode.erase(to_decode.begin());
    }

    // totalAllocQSize += allocQueue->size();
    // // cout<<"allocQueue size="<<allocQueue->size()<<"\n";
    // totalAllocTime+=1;

    // if (!allocQueue->isStall())
    // {
    //     allocQueue->sendInstToRename();
    // }
}

void Core::rename()
{
    if (flushStall)
        return;

    if (insnQueue.size() > 0)
        return;

    int renameWidth = window->retireSize;
    for (int j = 0; j < renameWidth; j++)
    {
        if (to_rename.size() == 0)
            break;
        if (insnQueue.size() >= renameWidth)
            break;
        auto inst = to_rename[0];

        // uidDynOpMap mapping static uid to most recent dynop
        // because we just want to build dependency to last insnance (dynop) of static insn
        uidDynOpMap[inst->getUID()] = inst;
        auto deps = inst->getStaticDepUIDs();
        for (auto dep : deps)
        {
            if (uidDynOpMap.find(dep) != uidDynOpMap.end())
            {
                auto dynOp = uidDynOpMap[dep];
                uint64_t dynID = dynOp->getDynID();
                // inst->addDynDeps(dynID);
                inst->addDep(dynOp);
            }
        }

        insnQueue.push_back(inst);
        to_rename.erase(to_rename.begin());
    }

    // while(!allocQueue->hasNoAvalInstToRename()) {

    //     auto insn = allocQueue->getInstToRename();

    //     // uidDynOpMap mapping static uid to most recent dynop
    //     // because we just want to build dependency to last insnance (dynop) of static insn
    //     uidDynOpMap[insn->getUID()] = insn;

    //     auto deps = insn->getStaticDepUIDs();
    //     for (auto dep : deps)
    //     {
    //         if (uidDynOpMap.find(dep) != uidDynOpMap.end())
    //         {
    //             auto dynOp = uidDynOpMap[dep];
    //             uint64_t dynID = dynOp->getDynID();
    //             // insn->addDynDeps(dynID);
    //             insn->addDep(dynOp);
    //         }
    //     }

     
    //     insnQueue.push_back(insn);
    // }
}

void Core::fetch()
{
    if (flushStall)
        return;

    if (to_decode.size() > 0)
        return;

    // if flush we must execute flushed insns before any other insns
    if (flushedInsns.size())
    {
        processedOps.insert(processedOps.begin(), flushedInsns.begin(), flushedInsns.end());
        flushedInsns.clear();
    }

    int fetchWidth = window->windowSize;
    int cl_size = params->l1i_CLSize;
    int n_inst = cl_size / 4;

    std::vector<std::shared_ptr<DynamicOperation>> bbOps;

    bool shouldStall = false;

    // Hard coded instbuffer size
    // TODO: fix it, and move after fetch
    bool shouldFetch = (fetchBuffer.size() <= (fetchWidth - to_decode.size()) || fetchBuffer.size() == 0);
    bool notWaiting = shouldFetch && (FTQ.size() == 0);

    // receive instructions from IFU
    ifu->IF1(processedOps, fetchBuffer, insnQueue, fetchCount, cycles, insnQueueMaxSize);

    // If branch is mispredicted, we should not fetch new instructions until the branch decision is made.
    if (lastOp)
    {
        if (lastOp->isBr() && lastOp->mispredict)
        {
            // Since we update BP at retire time,
            // when a branch is finished, the BP is not updated yet. 
            //  And we use the old BP to predict again in preprocess(),
            // the accuracy will be affected.
            // So we stall here until the branch is retired,
            // which looks better.
            // FIX THIS: correct with <
            if (lastOp->status <= DynOpStatus::FINISHED)
            {
                mispredict_stalls_fetch++;
                // cout<<"lastop="<<lastOp->getOpName()<<" dynid="<<lastOp->getDynID()<<" mispredict, waiting for branch decision\n";
                notWaiting = false;
            }
            else if (lastOp->status > DynOpStatus::FINISHED)
            {
                shouldStall = true;
                lastOp = nullptr;
            }
        }
    }

    if (notWaiting)
    {
        auto ops = preprocess();
        // if(found)
        bbOps.insert(bbOps.end(), ops.begin(), ops.end());
    }

    if (bbOps.size())
    {
        lastOp = bbOps[bbOps.size() - 1];
    }

    uint64_t cache_line_addr = 0;
    // Split instructions into cache lines
    // and now we know how many instructions in each cache line
    for (int i = 0; i < bbOps.size();)
    {
        FTQ.push_back(bbOps[i]);
        cache_line_addr = get_cache_line_addr(bbOps[i]->getUID(), n_inst);

        bool find = false;

        for (int j = i + 1; j < min((int)bbOps.size(), i + n_inst); j++)
        {
            if (bbOps[j]->getUID() >= cache_line_addr + n_inst)
            {
                assert(j - i <= n_inst);
                fetchCount.push(j - i);
                find = true;
                break;
            }
        }

        if (!find)
        {
            if (bbOps.size() - i > n_inst)
            {
                fetchCount.push(n_inst);
            }
            else
            {
                assert(bbOps.size() - i <= n_inst);
                fetchCount.push(bbOps.size() - i);
            }
        }

        int temp_n = fetchCount.back();
        for (int j = i; j < i + temp_n; j++)
        {
            processedOps.push_back(bbOps[j]);
        }
        i += fetchCount.back();
    }

    if (FTQ.size() > 0)
    {
        // stall due to branch misprediction
        if (shouldStall && flushPenalty > 0)
        {
            FTQ[0]->flushStall = true;
            rb_flushedN += 1;

            flushStallCycle = flushPenalty;
            flushStall = true;
            flushN++;
        }

        // fetch req to ICache
        if (!FTQ[0]->flushStall)
        {
            if (FTQ[0]->getDynID() != 0 && last_fetch_cache_line == get_cache_line_addr(FTQ[0]->getUID(), n_inst) && !ifu->isOccupied() && !FTQ[0]->mispredict)
            {
                ifu->fakeIF(FTQ, processedOps, fetchBuffer, fetchCount, cycles);
            }
            else
            {
                if (shouldFetch)
                {
                    last_fetch_cache_line = get_cache_line_addr(FTQ[0]->getUID(), n_inst);
                    ifu->IF0(FTQ, cycles);
                }
            }
        }else {
            flush_stalls_fetch++;
        }
    } else {
        ftq0_stalls_fetch++;
    }

    if (fetchBuffer.size() == 0)
        stalls_fetch++;

    // to decode
    for (int j = 0; j < fetchWidth; j++)
    {
        if (fetchBuffer.size() == 0)
            break;

        if (to_decode.size() >= fetchWidth)
            break;

        fetchBuffer[0]->F1 = cycles;
        to_decode.push_back(fetchBuffer[0]);
        instList.push_back(fetchBuffer[0]);

        // remove the op from fetchBuffer
        fetchBuffer.erase(fetchBuffer.begin());
    }
}

void Core::readCosimMemArgs(std::shared_ptr<CallDynamicOperation> call) {


    call->dram = readAddr();
    call->sp = readAddr() & sp_addr_mask;
    call->cols = readInt();
    call->rows = readInt();

    // std::cout<<"dram "<<std::hex<<call->dram<<"\n";
    // std::cout<<"sp "<<std::hex<<call->sp<<"\n";
    // std::cout<<"cols "<<std::dec<<call->cols<<"\n";
    // std::cout<<"rows "<<std::dec<<call->rows<<"\n";
}

void Core::readCosimArgs(std::shared_ptr<CallDynamicOperation> call) {
    std::vector<uint64_t> args;

    uint64_t T_N = readInt();
    uint64_t T_K = readInt();
    uint64_t T_M = readInt();

    uint64_t addr_MK = readAddr();
    uint64_t addr_MN = readAddr();
    uint64_t addr_KN = readAddr();

    // std::cout<<"T_N "<<std::hex<<T_N<<"\n";
    // std::cout<<"T_K "<<std::hex<<T_K<<"\n";
    // std::cout<<"T_M "<<std::hex<<T_M<<"\n";
    // std::cout<<"addr_MK "<<std::hex<<addr_MK<<"\n";
    // std::cout<<"addr_MN "<<std::hex<<addr_MN<<"\n";
    // std::cout<<"addr_KN "<<std::hex<<addr_KN<<"\n";

    // DO NOT forget this
    call->setRealAddr(addr_MK, addr_KN, addr_MN);

    if(call->isCosimConfigFunc) {
    
        uint64_t N = readInt();
        uint64_t K = readInt();
        uint64_t M = readInt();
        
        real_mk_ptrs.push(addr_MK);
        real_kn_ptrs.push(addr_KN);
        real_mn_ptrs.push(addr_MN);

        call->setConfigParams(N, M, K, 0, 0, 0, T_N, T_M, T_K);

    } else {

        call->sp_a_addr = readAddr() & sp_addr_mask;
        call->sp_c_addr = readAddr() & sp_addr_mask;
        call->sp_b_addr = readAddr() & sp_addr_mask;

        // std::cout<<"sp_a_addr "<<std::hex<<call->sp_a_addr<<"\n";
        // std::cout<<"sp_c_addr "<<std::hex<<call->sp_c_addr<<"\n";
        // std::cout<<"sp_b_addr "<<std::hex<<call->sp_b_addr<<"\n";

        // uint64_t offset_to_MK = (addr_MK - real_mk_ptrs.back()) / DATA_SIZE;
        // uint64_t offset_to_KN= (addr_KN - real_kn_ptrs.back()) / DATA_SIZE;
        // uint64_t offset_to_MN = (addr_MN - real_mn_ptrs.back()) / DATA_SIZE;
        uint64_t offset_to_MK = 0;
        uint64_t offset_to_KN= 0;
        uint64_t offset_to_MN = 0;

        // std::cout<<std::hex<<real_mk_ptrs.back() << " "<<addr_MK<<"\n";
        // std::cout<<std::hex<<real_kn_ptrs.back() << " "<<addr_KN<<"\n";
        // std::cout<<std::hex<<real_mn_ptrs.back() << " "<<addr_MN<<"\n";

        // std::cout<<std::dec<<"===== offset_to_MK="<<offset_to_MK << " offset_to_KN="<<offset_to_KN << " offset_to_MN="<<offset_to_MN<<"\n";

        call->setMatrixParams(T_N, T_M, T_K, offset_to_MK, offset_to_KN, offset_to_MN);
    }
    // std::cout<<std::dec;
}

// perfect branch prediction
std::vector<std::shared_ptr<DynamicOperation>> Core::preprocess()
{
    std::vector<std::shared_ptr<DynamicOperation>> ops;

    if (processedOps.size() >= insnQueueMaxSize * 2)
    {
        return ops;
    }

    fetchOpBuffer.clear();
    if (fetchForRet)
    {
        // main ret if size == 0
        if (returnStack.size() != 0)
        {
            auto retOps = returnStack.top();
            fetchOpBuffer = retOps->ops;
            bool isInvoke = retOps->isInvoke;
            if (isInvoke)
            {
                if (!task->at_end())
                {
                    string bbname = readBBName();
                }
            }
            returnStack.pop();
            delete retOps;

            previousBB = prevBBStack.top();
            prevBBStack.pop();

            prevBBTerminator = prevBBTermStack.top();
            prevBBTermStack.pop();
        }
        else
        {
            // in simpoint, the simulator may not know
            // return address of a ret insn, so we fetch
            // bb according to trace
            if (simpoint)
            {
                // cout << "ret fetch bb\n";
                targetBB = fetchTargetBB();
                // assert(targetBB);
                if (targetBB)
                {
                    fetchOpBuffer = scheduleBB(targetBB);
                }
                else
                {
                    cout << " warning:  ret fetch empty bb !!!\n";
                }
            }
        }
    }
    else
    {
        if (targetBB)
        {
            fetchOpBuffer = scheduleBB(targetBB);
        }
    }
    // targetBB = nullptr;
    fetchForRet = false;
    bool newBBScheduled = false;

    if (abnormalExit)
    {
        targetBB = nullptr;
        fetchOpBuffer.clear();
        return ops;
    }

    if (fetchOpBuffer.size() == 0)
    {
        return ops;
    }

    if (prevBBTerminator)
    {
        if (prevBBTerminator->isConditional())
        {
            fetchOpBuffer[0]->prevBBTerminator = prevBBTerminator;
        }
    }

    // add to FTQ according to IFU->getPackageSize
    for (int i = 0; i < fetchOpBuffer.size(); i++)
    {
        auto insn = fetchOpBuffer[i];
        insn->updateDynID(globalDynID);
        globalDynID++;
        ops.push_back(insn);
        // insnQueue.push_back(insn);

        totalFetch++;

        // cout << insn->getOpName() << "\n";
        // cout << insn_to_string_1(insn->getStaticInsn()->getLLVMInsn()) << "\n";

        if (insn->isBr())
        {
            // std::cout << "br inst\n";
            auto br = std::dynamic_pointer_cast<BrDynamicOperation>(insn);
            int cond = true;

            llvm::BranchInst *llvm_br = llvm::dyn_cast<llvm::BranchInst>(insn->getStaticInsn()->getLLVMInsn());
            if (llvm_br->isConditional())
            {
                if (task->at_end())
                {
                    abnormalExit = true;
                    break;
                }

                cond = readBrCondition();
                // std::cout<<cond<<"\n";
                // br->mispredict = !bp->predict(insn->getUID(), cond);

                // br->pred = bp->predict(insn->getUID(), cond);
                br->pred = bp->predict(insn->getUID(), 0);
                br->mispredict = (br->pred != cond);

                // speculatively update
                // Since we don't have a repair mechanism for the BP,
                // we only update it when the prediction is correct.
                // Mispredictions update at reitre time
                if(!br->mispredict) {
                    bp->updateBP(br->getUID(), cond, br->pred);
                }
            }

            br->setCondition(cond);
            newBBScheduled = true;
            previousBB = targetBB;
            targetBB = fetchTargetBB(br);

            if (!targetBB)
            {
                newBBScheduled = false;
            }

            prevBBTerminator = insn;

            // std::cout << "branch target BB name=" << targetBB->getBBName() << "\n";
            break;
        }
        else if (insn->isCall())
        {
            // std::cout << "call inst\n";
            auto call = std::dynamic_pointer_cast<CallDynamicOperation>(insn);
            previousBB = targetBB;

            prevBBTerminator = insn;
            targetBB = fetchTargetBB(call);

            if (call->isCosimFunc && !call->isCosimFence && !call->isFake) {
                assert(targetBB == nullptr);
                // std::cout<<call->getCalledFunc()->getName().str()<<"\n";
                if (!call->isCosimMem) {
                    readCosimArgs(call);
                } else {
                    readCosimMemArgs(call);
                }
            }

            if (targetBB)
            {
                newBBScheduled = true;
                // std::cout << "function target BB name=" << targetBB->getBBName() << "\n";
                std::vector<std::shared_ptr<DynamicOperation>> returnBB(fetchOpBuffer.begin() + i + 1, fetchOpBuffer.begin() + fetchOpBuffer.size());
                returnStack.push(new RetOps(returnBB));
                prevBBStack.push(previousBB);

                prevBBTermStack.push(prevBBTerminator);

                fetchOpBuffer.clear();
                break;
            }
        }
        else if (insn->isInvoke())
        {
            auto invoke = std::dynamic_pointer_cast<InvokeDynamicOperation>(insn);
            previousBB = targetBB;
            prevBBTerminator = insn;

            // this get entry bb of called function
            targetBB = fetchTargetBB(invoke);

            // get normal dest bb
            llvm::Value *destValue = invoke->getDestValue();
            auto normalDestBB = task->fetchBB(destValue);
            newBBScheduled = true;

            if (targetBB)
            {
                // only branch to normal dest
                auto normalDestBBOps = scheduleBB(normalDestBB);

                returnStack.push(new RetOps(normalDestBBOps, true));
                prevBBStack.push(previousBB);

                prevBBTermStack.push(prevBBTerminator);

                break;
            }
            else // this function is declartion then invoke is like a br
            {
                if (!task->at_end())
                {
                    if (simpoint)
                    {
                        string bbname = readBBName();
                        // cout << bbname << "\n";
                    }
                }
                targetBB = normalDestBB;
                break;
            }
        }
        else if (insn->isRet())
        {
            // std::cout << "ret inst" << "\n";
            fetchForRet = true;
            break;
        }
        else if (insn->isSwitch())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "switch inst\n";
            auto sw = std::dynamic_pointer_cast<SwitchDynamicOperation>(insn);
            int cond = readSwitchCondition();
            sw->setCondition(cond);
            // std::cout << std::hex << cond << "\n";
            // std::cout << std::dec;
            newBBScheduled = true;
            previousBB = targetBB;
            prevBBTerminator = insn;
            targetBB = fetchTargetBB(sw);
            // std::cout << "switch target BB name=" << targetBB->getBBName() << "\n";
            break;
        }
        else if (insn->isLoad())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "load inst\n";
            auto load = std::dynamic_pointer_cast<LoadDynamicOperation>(insn);
            uint64_t addr = readAddr();
            load->setAddr(addr);
            // std::cout << std::hex << addr << "\n";
            // std::cout << std::dec;
        }
        else if (insn->isStore())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "store inst\n";
            auto store = std::dynamic_pointer_cast<StoreDynamicOperation>(insn);
            uint64_t addr = readAddr();
            store->setAddr(addr);
            // std::cout << std::hex << addr << "\n";
            // std::cout << std::dec;
        }
        else if (insn->isPhi())
        {
            auto phi = std::dynamic_pointer_cast<PhiDynamicOperation>(insn);
            if (previousBB)
                phi->setPreviousBB(previousBB);
        }
        else
        {
            // std::cout << "arith inst\n";
        }
    }

    if (!newBBScheduled || abnormalExit)
        targetBB = nullptr;

    return ops;
}

void Core::insnToIssueQueues(std::vector<std::shared_ptr<DynamicOperation>> &insnToIssue)
{
    for (auto insn : insnToIssue)
    {
        if(insn->isCosimFunc) {
            cosimIssueQueue.push_back(insn);
        } else {
            issueQueue.push_back(insn);
        }
    }
}

void Core::issue()
{
    if (flushStall)
        return;

    auto readyInsns = issueUnit->issue(issueQueue, window, ports, lsu, cycles);

    for(auto insn : readyInsns) {
        if(insn->finish()) {
            insn->E = cycles;
            insn->status = DynOpStatus::FINISHED;
        }
    }
    
    // issue cosim insn one per cycle
    DynInstPtr front = cosimIssueQueue.size() > 0 ? cosimIssueQueue[0] : nullptr;
    if (front && window->canIssue(front) && stonne->get_sac()->is_inst_queue_available()) {
      cosimIssueQueue.erase(cosimIssueQueue.begin());
      window->usedCosimQueueSize--;
      readyInsns.push_back(front);

      assert(front->status == DynOpStatus::ALLOCATED);
      front->updateStatus(DynOpStatus::ISSUED);
      front->e = cycles;

      cosim_issue(front);
    }

    if (window->ifROBEmpty())
    {
        rs_empty_cycles++;
    }

    if (readyInsns.size() == 0)
    {
        if (lsu->getExecutingLoads() > 0)
        {
            stalls_mem_any++;
        }
        stalls_total++;
    }

    if (readyInsns.size() >= 3)
    {
        cycles_ge_3_uop_exec++;
    }

    if (readyInsns.size() >= 2)
    {
        cycles_ge_2_uop_exec++;
    }

    if (readyInsns.size() >= 1)
    {
        cycles_ge_1_uop_exec++;
    }
    // std::cout << "cycle=" << cycles << " readyInsns.sze()" << readyInsns.size() << "\n";
    inFlightInsn.insert(inFlightInsn.end(), readyInsns.begin(), readyInsns.end());
}

void Core::dispatch()
{
    if (flushStall)
        return;

    std::vector<std::shared_ptr<DynamicOperation>> dispatchedInsns;
    // uint64_t ws = window->getWindowSize();
    int dispatchWidth = window->windowSize;
    // for (int i = 0; i < dispatchWidth; i++)
    int dispatched = 0;
    while(dispatched < dispatchWidth)
    {
        if (insnQueue.size() == 0)
            break;

        auto insn = insnQueue[0];

        if (window->ifROBFull())
        {
            rob_full++;
            if(window->stallLoad()) {
                stall_load++;
            }
        }

        if(!window->canDispatch(insn)) {
            iq_full++;
        }

        // check if issuequeue, rob, and ld/st queue is full
        if (window->canDispatch(insn) && !window->ifROBFull() && lsu->canDispatch(insn))
        {
            dispatchedInsns.push_back(insn);

            if(insn->isCosimFunc) {
                window->usedCosimQueueSize++;
            } else {
                window->issueQueueUsed++;
            }

            assert(insn->status == DynOpStatus::INIT);
            insn->updateStatus();

            // to rob
            window->addInstruction(insn);
            // allocate entry in lsu
            lsu->addInstruction(insn);

            insnQueue.erase(insnQueue.begin());
            insn->D = cycles;

            ++dispatched;
        }
        else
        {
            break;
        }
    }
    insnToIssueQueues(dispatchedInsns);
}

void Core::writeBack()
{
    if (flushStall)
        return;

    lsu->wb();

    int wbWidth = window->retireSize;
    int temp_n = 0;
    for (uint64_t i = 0; i < inFlightInsn.size(); i++)
    {
        auto insn = inFlightInsn[i];

        if(insn->isCosimFunc) {

            insn->E = cycles;
            // cosim insn finish in execute stage
            continue;
        }

        // skip load/store that are commited
        if (insn->status == DynOpStatus::COMMITED)
            continue;

        // some insns may be already in FINISHED
        // such load and store is set finsihed in lsu->process
        // and insns complete but no commit
        assert(insn->status == DynOpStatus::EXECUTING || 
            insn->status == DynOpStatus::SLEEP || 
            insn->status == DynOpStatus::FINISHED);
        // if (insn->status == DynOpStatus::EXECUTING && insn->finish())
        if (insn->status != DynOpStatus::FINISHED && insn->finish())
        {
            assert(insn->status == DynOpStatus::EXECUTING);
            insn->E = cycles;
            insn->status = DynOpStatus::FINISHED;


            temp_n += 1;
        }

        // cstm insn free fu at stage cycle
        // if (insn->freeFUEarly())
        // {
        //     issueUnit->freeFU(insn->usingFuIDs, insn->usingPorts);
        //     insn->freeFU();
        // }

        if (insn->status == DynOpStatus::FINISHED)
        {
            issueUnit->freeFU(insn->usingFuIDs, insn->usingPorts);
            insn->freeFU();
        }

        // if (temp_n >= wbWidth)
        //     break;
    }
}

void Core::IE()
{
    issue();
    execute();
}

void Core::cosim_issue(DynInstPtr insn) {
    if (insn->isCosimFunc) {
        cosim_insn_num++;
        // insn->updateStatus(DynOpStatus::FINISHED);
        if (insn->isCosimConfigFunc) {
            // std::cout << "cosim config issue dynisn=" << insn->getDynID() << "\n";
            // std::cout<<"T_N="<<insn->get_T_N()<<" T_K="<<insn->get_T_K()<<" T_M="<<insn->get_T_M()<<"\n";
            // std::cout<<"addr_MK="<<std::hex<<insn->get_addr_MK()<<" addr_KN="<<insn->get_addr_KN()<<" addr_MN="<<insn->get_addr_MN()<<"\n";
            // std::cout<<"N="<<std::dec<<insn->get_N()<<" K="<<insn->get_K()<<" M="<<insn->get_M()<<"\n";
            Command* conf = new Command(insn->getDynID(), EXE, CONFIG, insn);
            conf->set_params(insn->get_T_N(), insn->get_T_K(), insn->get_T_M(),
                             insn->get_addr_MK(), insn->get_addr_KN(),
                             insn->get_addr_MN(), insn->get_N(), insn->get_K(),
                             insn->get_M());
            stonne->get_sac()->receive(conf);
        } else if (insn->isCosimFence) {
            // std::cout << "cosim fence issue dynisn=" << insn->getDynID() << "\n";
            Command* fence = new Command(insn->getDynID(), EXE, FENCE, insn);
            stonne->get_sac()->receive(fence);
        } else if (insn->isMatrixMultiply) {
            // std::cout << "cosim matmul issue dynisn=" << insn->getDynID() << "\n";
            // std::cout<<"T_N="<<insn->get_T_N()<<" T_K="<<insn->get_T_K()<<" T_M="<<insn->get_T_M()<<"\n";
            // std::cout<<"MK_offset="<<insn->get_MK_offset()<<" NK_offset="<<insn->get_KN_offset()<<" MN_offset="<<insn->get_MN_offset()<<"\n";
            Command* mm =
                new Command(insn->getDynID(), EXE, MATRIX_MULTIPLY, insn);
            mm->set_params(insn->get_T_N(), insn->get_T_K(), insn->get_T_M(),
                           insn->get_MK_offset(), insn->get_MN_offset(),
                           insn->get_KN_offset(), insn->sp_a_addr, insn->sp_b_addr, insn->sp_c_addr);
            mm->set_stay(false);

            stonne->get_sac()->receive(mm);
            cosim_mm_num++;
        } else if (insn->isMatrixMultiplyStay) {
            // std::cout << "cosim matmul stay issue dynisn=" << insn->getDynID() << "\n";
            // std::cout<<"T_N="<<insn->get_T_N()<<" T_K="<<insn->get_T_K()<<" T_M="<<insn->get_T_M()<<"\n";
            // std::cout<<"MK_offset="<<insn->get_MK_offset()<<" NK_offset="<<insn->get_KN_offset()<<" MN_offset="<<insn->get_MN_offset()<<"\n";
            Command* mm =
                new Command(insn->getDynID(), EXE, MATRIX_MULTIPLY, insn);
            mm->set_params(insn->get_T_N(), insn->get_T_K(), insn->get_T_M(),
                           insn->get_MK_offset(), insn->get_MN_offset(),
                           insn->get_KN_offset(), insn->sp_a_addr, insn->sp_b_addr, insn->sp_c_addr);
            mm->set_stay(true);

            stonne->get_sac()->receive(mm);
            cosim_mm_num++;
        } else if (insn->isMatrixMultiplyFlip) {
            // std::cout << "cosim matmul flip issue dynisn=" << insn->getDynID() << "\n";
            // std::cout<<"T_N="<<insn->get_T_N()<<" T_K="<<insn->get_T_K()<<" T_M="<<insn->get_T_M()<<"\n";
            // std::cout<<"MK_offset="<<insn->get_MK_offset()<<" NK_offset="<<insn->get_KN_offset()<<" MN_offset="<<insn->get_MN_offset()<<"\n";
            Command* mmf =
                new Command(insn->getDynID(), EXE, MATRIX_MULTIPLY_FLIP, insn);
            mmf->set_params(insn->get_T_N(), insn->get_T_K(), insn->get_T_M(),
                            insn->get_MK_offset(), insn->get_MN_offset(),
                            insn->get_KN_offset(), insn->sp_a_addr, insn->sp_b_addr, insn->sp_c_addr);
            stonne->get_sac()->receive(mmf);
            cosim_mm_num++;
        } else if (insn->isPreload) {
            // std::cout << "cosim preload issue dynisn=" << insn->getDynID() << "\n";
            // std::cout<<"T_N="<<insn->get_T_N()<<" T_K="<<insn->get_T_K()<<" T_M="<<insn->get_T_M()<<"\n";
            // std::cout<<"MK_offset="<<insn->get_MK_offset()<<" NK_offset="<<insn->get_KN_offset()<<" MN_offset="<<insn->get_MN_offset()<<"\n";
            Command* preload =
                new Command(insn->getDynID(), EXE, PRELOAD, insn);
            preload->set_params(insn->get_T_N(), insn->get_T_K(),
                                insn->get_T_M(), insn->get_MK_offset(),
                                insn->get_MN_offset(), insn->get_KN_offset(), insn->sp_a_addr, insn->sp_b_addr, insn->sp_c_addr);
            stonne->get_sac()->receive(preload);
            cosim_preload_num++;
        } else if (insn->isMvin) {
            Command* mvin = new Command(insn->getDynID(), LOAD, MOVE_IN, insn);
            mvin->set_params(insn->dram, insn->sp, insn->cols, insn->rows);
            stonne->get_sac()->receive(mvin);

        } else if (insn->isMvout) {
            Command* mvout = new Command(insn->getDynID(), STORE, MOVE_OUT, insn);
            mvout->set_params(insn->dram, insn->sp, insn->cols, insn->rows);
            stonne->get_sac()->receive(mvout);

        } else if (insn->isMvinAcc) {
            Command* mvin = new Command(insn->getDynID(), LOAD_ACC, MOVE_IN_ACC, insn);
            mvin->set_params(insn->dram, insn->sp, insn->cols, insn->rows);
            stonne->get_sac()->receive(mvin);
        } else if(insn->isFake) {
            Command* fake = new Command(insn->getDynID(), EXE, FAKE, insn);
            stonne->get_sac()->receive(fake);
        }

        else {
            assert(false && "unknown cosim func");
        }
    }
}

void Core::execute()
{
    if (flushStall)
        return;

    for (uint64_t i = 0; i < inFlightInsn.size(); i++)
    {
        auto insn = inFlightInsn[i];
        if (insn->isCosimFunc && !insn->isCosimFence) {
            insn->updateStatus(DynOpStatus::FINISHED);
            continue;
        }

        if (insn->status != DynOpStatus::FINISHED)
        {
            insn->execute();

            if(insn->isLoad()) {
                lsu->executeLoad(insn);
            }

            if(insn->isStore()) {
                lsu->executeStore(insn);
            }

            if (insn->status == DynOpStatus::SLEEP)
            {
                issueUnit->freeFU(insn->usingFuIDs, insn->usingPorts);
                insn->freeFU();
            }
        }
    }
}

bool compareInsn(const std::shared_ptr<DynamicOperation> a, const std::shared_ptr<DynamicOperation> b)
{
    return a->getDynID() < b->getDynID();
}

void Core::commit()
{
    if (flushStall)
        return;

    uint64_t flushDynID = 0;
    // flush after insn (exclusive) of flushDynID because of memory disambiguation

    window->retireInstruction(bp, stat, lsu, flushDynID, cycles, funcCycles);
    lsu->commitInstruction(window->doneSeqNum);
    commitInsn(window->doneSeqNum);

    // remove ops that has finished and commited
    for (auto it = inFlightInsn.begin(); it != inFlightInsn.end();)
    {
        std::shared_ptr<DynamicOperation> insn = *it;
        if (insn->status == DynOpStatus::COMMITED)
        {
            if(insn->isLoad() || insn->isStore())
                if(insn->isSent())
                    it = inFlightInsn.erase(it);
                else
                    it++;
            else
                it = inFlightInsn.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void Core::run()
{

    commit();
    // IE();

    stonne->cycle();
    execute();
    writeBack();

    issue();

    dispatch();

    // IE();
    rename();

    decode();

    fetch();

    // insert bubble for flush
    if (flushStall)
    {
        // flush the entire cycle
        if (flushStallCycle > 0)
        {
            flushStallCycle--;
        }
        assert(flushStallCycle >= 0);
        flushStall = false;
        FTQ[0]->flushStall = false;
    }

    cycles++;

    l1i->process();
    l1c->process();
    l2c->process();

    if (
        processedOps.size() == 0 &&
        insnQueue.size() == 0 &&
        issueQueue.size() == 0 &&
        inFlightInsn.size() == 0 &&
        window->ifROBEmpty() &&
        flushedInsns.size() == 0 &&
        to_decode.size() == 0 &&
        to_rename.size() == 0 &&
        fetchBuffer.size() == 0 &&
        allocQueue->isEmpty() &&
        stonne->finished() &&
        allocQueue->hasNoAvalInstToRename() &&
        stonne->get_sac()->is_execution_finished())
        running = false;

    return;
}

uint64_t Core::getTotalCommit()
{
    return window->totalCommit;
}

uint64_t Core::get_cache_line_addr(uint64_t addr, int cache_line_size)
{

    return addr / cache_line_size * cache_line_size;
}

void Core::commitInsn(InstSeqNum doneSN)
{
    auto order_it = instList.begin();
    auto order_end_it = instList.end();

    while (order_it != order_end_it && (*order_it)->getDynID() <= doneSN)
    {
        DynInstPtr inst = *order_it;
        order_it = instList.erase(order_it);
    }
}
