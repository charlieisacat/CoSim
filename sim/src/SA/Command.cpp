#include "SA/Command.h"

void Command::compute_sp_addr_range() {

    this->rs2_start = this->sp_b_addr;
    this->rs2_end = this->sp_b_addr + this->T_N;

    if (get_op_type() == PRELOAD) {
        this->rs1_start = 0;
        this->rs1_end = 0;

        this->dst_start = 0;
        this->dst_end = 0;

    } else {
        this->rs1_start = this->sp_a_addr;
        this->rs1_end = this->sp_a_addr + this->T_M;

        this->dst_start = this->sp_c_addr;
        this->dst_end = this->sp_c_addr + this->T_M;
    }

    // std::cout << "cmd: " << get_seq_num() << " " << (get_op_type() == PRELOAD ? "PRELOAD" : "MM")
    //           << " rs1_start=" << this->rs1_start
    //           << " rs1_end=" << this->rs1_end
    //           << " rs2_start=" << this->rs2_start
    //           << " rs2_end=" << this->rs2_end
    //           << " dst_start=" << this->dst_start
    //           << " dst_end=" << this->dst_end << std::endl;
}

void Command::set_done() {
    this->status = COSIM_FINISHED;

    // if(this->dyn_inst && this->dyn_inst->isCosimFence) {
    //     this->dyn_inst->updateStatus(FINISHED);
    // }
    // if(this->dyn_inst) {
    //     this->dyn_inst->updateStatus(FINISHED);
    // }
}

bool Command::is_ready() {
    for (auto parent : this->parents) {
        if (parent->status < COSIM_FINISHED) {
            return false;
        }
    }
    return true;
}

void Command::set_params(tile_t T_N,
                         tile_t T_K,
                         tile_t T_M,
                         tile_t offset_to_MK,
                         tile_t offset_to_MN,
                         tile_t offset_to_KN,
                         uint64_t sp_a_addr,
                         uint64_t sp_b_addr,
                         uint64_t sp_c_addr) {
    this->T_N = T_N;
    this->T_K = T_K;
    this->T_M = T_M;
    this->offset_to_MK = offset_to_MK;
    this->offset_to_MN = offset_to_MN;
    this->offset_to_KN = offset_to_KN;
    this->sp_a_addr = sp_a_addr;
    this->sp_b_addr = sp_b_addr;
    this->sp_c_addr = sp_c_addr;

    compute_sp_addr_range();

    // std::cout << "Command::set_params: " << this->T_N << " " << this->T_K << " "
    //           << this->T_M << " " << this->offset_to_MK << " "
    //           << this->offset_to_MN << " " << this->offset_to_KN << " "
    //           << this->sp_addr_MK << " " << this->sp_addr_KN << " "
    //           << this->sp_addr_MN << std::endl;
}

// void Command::set_params(tile_t T_N,
//                          tile_t T_K,
//                          tile_t T_M,
//                          tile_t offset_to_MK,
//                          tile_t offset_to_MN,
//                          tile_t offset_to_KN) {
//     this->T_N = T_N;
//     this->T_K = T_K;
//     this->T_M = T_M;
//     this->offset_to_MK = offset_to_MK;
//     this->offset_to_MN = offset_to_MN;
//     this->offset_to_KN = offset_to_KN;

//     // std::cout << "Command::set_params: " << this->T_N << " " << this->T_K << " "
//     //           << this->T_M << " " << this->offset_to_MK << " "
//     //           << this->offset_to_MN << " " << this->offset_to_KN << std::endl;
// }

void Command::set_params(tile_t T_N,
                             tile_t T_K,
                             tile_t T_M,
                             address_t MK_address,
                             address_t KN_address,
                             address_t MN_address,
                             tile_t N, tile_t K, tile_t M) {
    this->T_N = T_N;
    this->T_K = T_K;
    this->T_M = T_M;
    this->MK_address = MK_address;
    this->KN_address = KN_address;
    this->MN_address = MN_address;
    this->N = N;
    this->K = K;
    this->M = M;

    // std::cout << "Command::set_params: " << this->T_N << " " << this->T_K << " "
    //           << this->T_M << " " << this->MK_address << " "
    //           << this->KN_address << " " << this->MN_address << std::endl;
}

// TODO: MAX_BLOCK_LEN
void Command::set_params(uint64_t dram_addr,
                         uint64_t sp_addr,
                         tile_t cols,
                         tile_t rows) {
    this->dram_addr = dram_addr;
    this->sp_addr = sp_addr;
    this->cols = cols;
    this->rows = rows;

    if (get_type() == LOAD || get_type() == LOAD_ACC) {
        this->dst_start = sp_addr;
        this->dst_end = sp_addr + rows;

        bool isAccBuf = (get_type() == LOAD_ACC);
        // std::cout << (isAccBuf ? "ACC" : "SPM") << " cmd: " << get_seq_num() << std::dec << " MOVE_IN: dst_start = " << this->dst_start << ", dst_end = " << this->dst_end << std::dec << " rows=" << rows << std::endl;
    } else if (get_type() == STORE) {
        this->rs1_start = sp_addr;
        this->rs1_end = sp_addr + rows;
        // std::cout << "cmd: " << get_seq_num() << std::dec << " MOVE_OUT: rs1_start = " << this->rs1_start << ", rs1_end = " << this->rs1_end << std::dec << " rows=" << rows << std::endl;
    }
}

bool Command::overlap(uint64_t addr_start, uint64_t addr_end, uint64_t other_start, uint64_t other_end) {
    return (addr_start < addr_end && other_start < other_end) && ((other_start <= addr_start && addr_start < other_end) || (addr_start <= other_start && other_start < addr_end));
}

// MM: rs1 and rs2 are in SPM, but dst are in AccBuf
// LOAD: dst is in SPM
// STORE: rs1 is in AccBuf
// LOAC_ACC: dst is in AccBuf
bool Command::overlap(Command* other) {
    if (this->get_type() == EXE) {
        if (other->get_type() == EXE) {
            // MM vs MM
            // As we have pipelined MM and MMs execute in sequence, this is not necessary
        } else if (other->get_type() == LOAD) {
            // MM vs LOAD
            return overlap(this->rs1_start, this->rs1_end, other->dst_start, other->dst_end) ||  // RAW
                   overlap(this->rs2_start, this->rs2_end, other->dst_start, other->dst_end);
        } else if (other->get_type() == STORE) {
            // MM vs STORE
            return overlap(this->dst_start, this->dst_end, other->rs1_start, other->rs1_end);  // WAR
        } else if (other->get_type() == LOAD_ACC) {
            // MM vs LOAD_ACC, load D into accbuf before MM writes to accbuf
            return overlap(this->dst_start, this->dst_end, other->dst_start, other->dst_end);  // WAW
        }
    } else if (this->get_type() == LOAD) {
        if (other->get_type() == EXE) {
            // LOAD vs MM
            return overlap(this->dst_start, this->dst_end, other->rs1_start, other->rs1_end) ||  // WAR
                   overlap(this->dst_start, this->dst_end, other->rs2_start, other->rs2_end);
        } else if (other->get_type() == LOAD) {
            // LOAD vs LOAD
            return overlap(this->dst_start, this->dst_end, other->dst_start, other->dst_end);  // WAW
        } else if (other->get_type() == STORE) {
            // LOAD vs STORE
            // NO NEED TO CHECK
        } else if (other->get_type() == LOAD_ACC) {
        }
    } else if (this->get_type() == STORE) {
        if (other->get_type() == EXE) {
            // STORE vs MM
            return overlap(this->rs1_start, this->rs1_end, other->dst_start, other->dst_end);  // WAR
        } else if (other->get_type() == LOAD) {
            // STORE vs LOAD
            // NO NEED TO CHECK
        } else if (other->get_type() == STORE) {
            // STORE vs STORE
            // NO NEED TO CHECK
        } else if (other->get_type() == LOAD_ACC) {
            // Store right after LOAD_ACC ?
            return overlap(this->rs1_start, this->rs1_end, other->dst_start, other->dst_end);  // WAR
        }
    } else if (this->get_type() == LOAD_ACC) {
        if (other->get_type() == EXE) {
            // LOAD_ACC vs MM
            return overlap(this->dst_start, this->dst_end, other->dst_start, other->dst_end);  // WAW
        } else if (other->get_type() == LOAD) {
            // LOAD_ACC vs LOAD
        } else if (other->get_type() == STORE) {
            // LOAD_ACC vs STORE
            return overlap(this->dst_start, this->dst_end, other->rs1_start, other->rs1_end);  // WAR
        } else if (other->get_type() == LOAD_ACC) {
            // LOAD_ACC vs LOAD_ACC
            return overlap(this->dst_start, this->dst_end, other->dst_start, other->dst_end);  // WAW
        }
    } else {
        assert(false && "unknown command type in overlap check");
    }

    return false;
}