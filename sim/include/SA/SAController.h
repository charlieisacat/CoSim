#ifndef SACONTROLLER_H
#define SACONTROLLER_H
#include <cassert>
#include <cstdint>
#include "Command.h"
#include "MemoryController.h"
#include "MultiplierNetwork.h"
#include "WSAccumulationBuffer.h"
#include "types.h"

#include "STONNEModel.h"

#include "mem/DMAEngine.h"

class Cache;

class Stonne;

#include <list>
#include <deque>
#include <memory>

class MemoryController;
class MultiplierNetwork;
class WSAccumulationBuffer;
class Command;

class BaseQueue {
   private:
    std::list<Command*> cmd_list;
    int capacity;
    int used_entries;

   public:
    BaseQueue(int size) : capacity(size), used_entries(0) {}
    void commit_command(CmdSeqNum seq_num);
    void insert_command(Command* cmd);
    bool is_full() { return used_entries == capacity; }
    bool is_empty() { return used_entries == 0; }
    Command* front() { return cmd_list.front(); }
    Command* back() { return cmd_list.back(); }
    void pop_front();
    void remove(Command* cmd);
    Command* get_cmd_at(int index);

    std::list<Command*> get_cmd_list() { return cmd_list; }
    int get_used_capacity() { return used_entries; }
};

class IssueQueue : public BaseQueue {
   public:
    IssueQueue(int size) : BaseQueue(size) {}
};

class ROB : public BaseQueue {
   private:
    int commit_width;
    CmdSeqNum commit_seq_num;
    bool cmd_commit = false;
    cycles_t local_cycle = 0;

   public:
    ROB(int size, int width) : commit_width(width), BaseQueue(size) {}
    Command* get_commit_head();
    void commit();

    CmdSeqNum get_commit_seq_num() { return commit_seq_num; }
    bool is_cmd_commit() { return cmd_commit; }
    void reset_cmd_commit() { cmd_commit = false; }

    void build_data_dep(Command* cmd);
};

typedef IssueQueue Controller;
typedef IssueQueue InstQueue;

class SAController {
   private:
    IssueQueue load_queue;
    IssueQueue store_queue;
    IssueQueue exe_queue;

    Controller load_contlr;
    Controller store_contlr;
    Controller exe_contlr;

    ROB rob;

    InstQueue inst_queue;  // insts from CPU

    cycles_t local_cycle;

    MultiplierNetwork* msnet;
    MemoryController* mem;
    WSAccumulationBuffer* wsAB;

    std::map<OperationType, bool> fu_idle;

    DataflowType dataflow;
    std::deque<Command*> executing_preload_cmds;
    std::deque<Command*> executing_mm_cmds;
    bool fence_active = false;
    Command* executing_fence_cmd = nullptr;

    Stonne* stonne = nullptr;
    bool preloading = false;

    DMAEngine dma_engine;

   public:
    SAController(DataflowType _dataflow)
        : load_queue(8),
          store_queue(2),
          exe_queue(8),
          load_contlr(8),
          store_contlr(4),
          exe_contlr(16),
          rob(48, 1),
          inst_queue(2), dataflow(_dataflow) {
        msnet = nullptr;
        mem = nullptr;
        wsAB = nullptr;
        local_cycle = 0;

        for (int i = 0; i <= OperationType::END; i++) {
            fu_idle[(OperationType)i] = true;
        }
    }

    bool performing_mm = false;
    bool performing_preload = false; 

    void cycle();

    int get_inst_queue_size() { return inst_queue.get_used_capacity(); }
    Command* get_inst_queue_back() { return inst_queue.back(); }

    bool is_inst_queue_available() { return !inst_queue.is_full(); }

    // from CPU to inst_queue
    void receive(Command* cmd) { inst_queue.insert_command(cmd); }
    void dispatch(Command* cmd);  // from inst_queue to ROB/queues
    void issue(Command* cmd);     // from queues to controllers
    void execute(Command* cmd);   // compute
    void complete(OperationType op) { fu_idle[op] = true; }

    void load(Command* cmd);   // to SP
    void store(Command* cmd);   // to DRAM

    // pipeline
    void dispatch();
    void issue();
    void execute();
    void commit();
    bool can_release_fence();

    void set_preloading(bool val) { preloading = val; }

    // full check
    bool can_receive() { return !inst_queue.is_full(); }
    bool can_dispatch(Command* cmd);
    bool can_issue(Command* cmd) {
        return cmd->is_ready() && cmd->get_status() == COSIM_WAITING;
    }
    bool can_execute(Command* cmd) { 
        OperationType op = cmd->get_op_type();
        if (op == PRELOAD) {
            if(dataflow == DATAFLOW_OS) {
                // std::cout<<"preload can_execute check flip status="<<msnet->has_array_flipped()<<std::endl;
                // return fu_idle[op] && msnet->has_array_flipped() && !performing_mm;
                return fu_idle[op] && msnet->has_array_flipped();
            }
            
            if(dataflow == DATAFLOW_WS) {
                // std::cout<<"preload can_execute check flip status="<<msnet->has_array_flipped()<<std::endl;
                // return fu_idle[op] && msnet->has_array_first_row_flipped();
                return fu_idle[op] && msnet->has_array_flipped();
                // return fu_idle[op] && msnet->has_array_flipped() && !performing_mm;
            }

            return fu_idle[op];
        }
        if ((op == MATRIX_MULTIPLY || op == MATRIX_MULTIPLY_FLIP) &&
            dataflow == DATAFLOW_OS) {
                // std::cout<<"can_execute check flip status="<<msnet->has_array_flipped()<<std::endl;
            // return msnet->has_array_flipped() && mem->is_idle() && !preloading && fu_idle[op];
            // return msnet->has_array_flipped() && mem->is_idle() && !preloading;
            return msnet->has_array_flipped() && mem->is_idle();
        }

        if((op == MATRIX_MULTIPLY || op == MATRIX_MULTIPLY_FLIP) && dataflow == DATAFLOW_WS) {
            // return msnet->has_array_flipped() && !preloading;
            // return msnet->has_array_flipped() && mem->is_idle() && !preloading && fu_idle[op];
            // return msnet->has_array_flipped() && mem->is_idle() && !preloading;
            return msnet->has_array_flipped() && mem->is_idle();
        }

        if(op == MOVE_IN  || op == MOVE_IN_ACC) {
            return max_load_dma_counter < 2;
        }

        if(op == MOVE_OUT) {
            return max_store_dma_counter < 2;
        }
        return fu_idle[op]; 
    }

    void set_multiplier_network(MultiplierNetwork* msnet) {
        this->msnet = msnet;
    }
    void set_memory_controller(MemoryController* mem) { this->mem = mem; }
    void set_wsaccumulation_buffer(WSAccumulationBuffer* wsAB) {
        this->wsAB = wsAB;
    }

    void Preload(unsigned int T_N,
                 unsigned int T_K,
                 unsigned int offset,
                 Command* cmd);
    void MatrixMultiply(unsigned int T_M,
                        unsigned int T_K,
                        unsigned int T_N,
                        unsigned int _offset_to_MK,
                        unsigned int _offset_to_MN,
                        unsigned int _offset_to_KN,
                        Command* cmd,
                        bool flip = false);

    // WS
    bool weights_preloaded() { return msnet->weights_preloaded(); }
    bool psums_accumulated() { return wsAB->is_psums_accumulated(); }

    // OS
    bool compute_finished() { return mem->is_compute_finished(); }
    bool psums_received() { return msnet->psums_received(); }
    void try_complete_fence();

    bool is_execution_finished() {
        return inst_queue.is_empty() && load_queue.is_empty() &&
               store_queue.is_empty() && exe_queue.is_empty() &&
               load_contlr.is_empty() && store_contlr.is_empty() &&
               exe_contlr.is_empty() && rob.is_empty();
    }

    void set_stonne_model(Stonne* model) { this->stonne = model; }

    // Optional: enable real cache injection for DMA via the core's L2 cache.
    void connect_dma_to_l2(std::shared_ptr<Cache> l2_cache) {
        dma_engine.connectL2(std::move(l2_cache));
    }

    DMAEngine* get_dma_engine() { return &dma_engine; }

    void build_data_dep(Command* cmd);

    int max_load_dma_counter = 0;
    int max_store_dma_counter = 0;

    uint64_t exe_cycle = 0;
    uint64_t ld_cycle = 0;
    uint64_t st_cycle = 0;
    uint64_t ld_st_cycle = 0;
};

#endif