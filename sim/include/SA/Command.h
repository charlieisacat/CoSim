#ifndef COMMAND_H
#define COMMAND_H
#include <cstdint>
#include <vector>
#include "types.h"
#include <iostream>

#include "../task/dyn_insn.h"

class Command {
   private:
    CmdSeqNum seq_num;
    CommandType cmd_type;
    CommandStatus status;
    OperationType op_type;

    std::vector<Command*> parents;

    // params
    tile_t T_N, T_K, T_M;
    tile_t N, K, M;
    tile_t offset_to_MK, offset_to_MN, offset_to_KN;
    address_t MK_address, KN_address, MN_address;

    // mvin/mvout
    uint64_t dram_addr;
    uint64_t sp_addr;
    tile_t cols;
    tile_t rows;

    uint64_t sp_a_addr;
    uint64_t sp_b_addr;
    uint64_t sp_c_addr;

    DynInstPtr dyn_inst;

    bool stay = false;

   public:
    Command(CmdSeqNum seq_num, CommandType _type, OperationType _op, DynInstPtr dyn_inst = nullptr)
        : seq_num(seq_num), cmd_type(_type), op_type(_op), status(COSIM_WAITING), dyn_inst(dyn_inst) {
        T_N = 0;
        T_K = 0;
        T_M = 0;
        N = 0;
        K = 0;
        M = 0;
        offset_to_MK = 0;
        offset_to_MN = 0;
        offset_to_KN = 0;
    }
    ~Command() {
        children.clear();
        parents.clear();
    }

    std::vector<Command*> children;

    void set_stay(bool val) { this->stay = val; }
    bool get_stay() { return this->stay; }

    CmdSeqNum get_seq_num() { return this->seq_num; }
    void set_done();

    bool can_commit() { return this->status == COSIM_FINISHED; }

    void insert_parent(Command* parent) { this->parents.push_back(parent); }

    void insert_child(Command* child) { this->children.push_back(child); }

    bool is_ready();
    CommandType get_type() { return this->cmd_type; }
    OperationType get_op_type() { return this->op_type; }
    void update_status(CommandStatus new_status) { this->status = new_status; }
    CommandStatus get_status() { return this->status; }

    void remove_parent(Command* parent) {
        this->parents.erase(
            std::remove(this->parents.begin(), this->parents.end(), parent),
            this->parents.end());
    }


    // get params
    tile_t get_T_N() { return this->T_N; }
    tile_t get_T_K() { return this->T_K; }
    tile_t get_T_M() { return this->T_M; }
    tile_t get_offset_to_MK() { return this->offset_to_MK; }
    tile_t get_offset_to_MN() { return this->offset_to_MN; }
    tile_t get_offset_to_KN() { return this->offset_to_KN; }
    tile_t get_N() { return this->N; }
    tile_t get_K() { return this->K; }
    tile_t get_M() { return this->M; }
    address_t get_MK_address() { return this->MK_address; }
    address_t get_KN_address() { return this->KN_address; }
    address_t get_MN_address() { return this->MN_address; }

    // mvin/mvout
    uint64_t get_dram_addr() { return this->dram_addr; }
    uint64_t get_sp_addr() { return this->sp_addr; }
    tile_t get_cols() { return this->cols; }
    tile_t get_rows() { return this->rows; }

    // sp
    uint64_t get_dst_start() { return this->dst_start; }
    uint64_t get_dst_end() { return this->dst_end; }
    uint64_t get_rs1_start() { return this->rs1_start; }
    uint64_t get_rs1_end() { return this->rs1_end; }
    uint64_t get_rs2_start() { return this->rs2_start; }
    uint64_t get_rs2_end() { return this->rs2_end; }

    void compute_sp_addr_range();

    // void set_params(tile_t T_N,
    //                 tile_t T_K,
    //                 tile_t T_M,
    //                 tile_t offset_to_MK,
    //                 tile_t offset_to_MN,
    //                 tile_t offset_to_KN);

    void set_params(tile_t T_N,
                    tile_t T_K,
                    tile_t T_M,
                    tile_t offset_to_MK,
                    tile_t offset_to_MN,
                    tile_t offset_to_KN,
                    uint64_t sp_a_addr,
                    uint64_t sp_b_addr,
                    uint64_t sp_c_addr);

    void set_params(tile_t T_N, tile_t T_K, tile_t T_M, address_t MK_address,
                    address_t KN_address, address_t MN_address, tile_t N,
                    tile_t K, tile_t M);

    DynInstPtr getDynInst() { return this->dyn_inst; }

    void set_params(uint64_t dram_addr,
                    uint64_t sp_addr,
                    tile_t cols,
                    tile_t rows);

    bool overlap(Command* other);
    bool overlap(uint64_t addr_start, uint64_t addr_end, uint64_t other_start, uint64_t other_end);

    uint64_t rs1_start = 0, rs1_end = 0;
    uint64_t rs2_start = 0, rs2_end = 0;
    uint64_t dst_start = 0, dst_end = 0;

    // 4 SPM ready lat + 2 hand-shaking lat
    int spm_delay = 4;
    int acc_delay = 2;

    void set_dyn_inst_finished() {
        if(this->dyn_inst && this->dyn_inst->isCosimFunc) {
            this->dyn_inst->updateStatus(FINISHED);
        }
    }
};

#endif