#include "SA/SAController.h"

#include <list>
#include <algorithm>

void BaseQueue::pop_front() {
    cmd_list.pop_front();
    used_entries--;
}

void BaseQueue::remove(Command* cmd) {
    cmd_list.remove(cmd);
    used_entries--;
}

Command* BaseQueue::get_cmd_at(int index) {
    assert(index < used_entries);
    auto it = cmd_list.begin();
    std::advance(it, index);

    return *it;
}

void BaseQueue::commit_command(CmdSeqNum seq_num) {
    auto order_it = cmd_list.begin();
    auto order_end_it = cmd_list.end();

    while (order_it != order_end_it && (*order_it)->get_seq_num() <= seq_num) {
        order_it = cmd_list.erase(order_it);
        used_entries--;
    }
}

void BaseQueue::insert_command(Command* cmd) {
    assert(used_entries < capacity);
    cmd_list.push_back(cmd);
    used_entries++;
}

Command* ROB::get_commit_head() {
    auto order_it = get_cmd_list().begin();
    Command* head = *order_it;

    if (head->can_commit()) {
        return head;
    }

    return nullptr;
}

void SAController::dispatch(Command* cmd) {
    build_data_dep(cmd);

    rob.insert_command(cmd);
    switch (cmd->get_type()) {
        case LOAD:
            load_queue.insert_command(cmd);
            // std::cout<<"Cycle=" << local_cycle << ", Dispatching LOAD command "
            //          << cmd->get_seq_num() << std::endl;
            break;
        case LOAD_ACC:
            load_queue.insert_command(cmd);
            // std::cout<<"Cycle=" << local_cycle << ", Dispatching LOAD ACC command "
            //          << cmd->get_seq_num() << std::endl;
            break;
        case STORE:
            store_queue.insert_command(cmd);
            // std::cout<<"Cycle=" << local_cycle << ", Dispatching STORE command "
            //          << cmd->get_seq_num() << std::endl;
            break;
        case EXE:
            // std::cout << "Cycle=" << local_cycle << ", Dispatching command "
            //           << cmd->get_seq_num() << " of type " << cmd->get_type()
            //           << std::endl;
            exe_queue.insert_command(cmd);
            break;
        default:
            assert(0);
    }
}

void SAController::issue(Command* cmd) {
    // std::cout << "Cycle=" << local_cycle << ", Issuing command "
    //           << cmd->get_seq_num() << " of type " << cmd->get_type()
    //           << std::endl;
    switch (cmd->get_type()) {
        case LOAD:
            load_contlr.insert_command(cmd);
            // std::cout<<"Cycle=" << local_cycle << ", Issuing LOAD command "
            //          << cmd->get_seq_num() << std::endl;
            break;
        case LOAD_ACC:
            load_contlr.insert_command(cmd);
            // std::cout<<"Cycle=" << local_cycle << ", Issuing LOAD ACC command "
            //          << cmd->get_seq_num() << std::endl;
            break;
        case STORE:
            store_contlr.insert_command(cmd);
            // std::cout<<"Cycle=" << local_cycle << ", Issuing STORE command "
            //          << cmd->get_seq_num() << std::endl;
            break;
        case EXE:
            // std::cout << "Cycle=" << local_cycle << ", Issuing command "
            //           << cmd->get_seq_num() << " of type " << cmd->get_type()
            //           << std::endl;
            exe_contlr.insert_command(cmd);
            break;
        default:
            assert(0);
    }
}

bool SAController::can_dispatch(Command* cmd) {
    if (rob.is_full()) {
        return false;
    }

    switch (cmd->get_type()) {
        case LOAD:
            return !load_queue.is_full();
        case LOAD_ACC:
            return !load_queue.is_full();
        case STORE:
            return !store_queue.is_full();
        case EXE:
            return !exe_queue.is_full();
        default:
            assert(0);
    }
}

void ROB::build_data_dep(Command* cmd) {
    for (auto it : get_cmd_list()) {
        Command *rob_cmd = it;

        if(rob_cmd->get_status() < COSIM_FINISHED && cmd->overlap(rob_cmd)) {
            // std::cout<<"Data dep between cmd "<<cmd->get_seq_num()<<" and rob_cmd "<<rob_cmd->get_seq_num()<<std::endl;
            cmd->insert_parent(rob_cmd);
            rob_cmd->insert_child(cmd);
        }
    }
}

void SAController::build_data_dep(Command* cmd) {
    rob.build_data_dep(cmd);
}

void SAController::dispatch() {
    if (!inst_queue.is_empty()) {
        if (fence_active) {
            // break;
            return;
        }
        Command* cmd = inst_queue.get_cmd_list().front();
        if (can_dispatch(cmd)) {
            cmd->update_status(COSIM_WAITING);
            dispatch(cmd);
            inst_queue.pop_front();
            // if (cmd->get_op_type() == FENCE || cmd->get_op_type() == CONFIG) {
            if (cmd->get_op_type() == FENCE) {
                // std::cout<<"Cycle=" << local_cycle
                //          << ", Dispatching FENCE/CONFIG command " << cmd->get_seq_num()
                //          << std::endl;
                fence_active = true;
            }
        } else {
            // break;
        }
    }
}

void SAController::issue() {
    if(!store_queue.is_empty() && !store_contlr.is_full()) {
        Command* store_cmd = store_queue.get_cmd_at(0);
        if(can_issue(store_cmd)) {
            store_cmd->update_status(COSIM_ISSUED);
            issue(store_cmd);
            store_queue.remove(store_cmd);
        }
    }


    if(!load_queue.is_empty() && !load_contlr.is_full()) {
        Command* load_cmd = load_queue.get_cmd_at(0);
        // std::cout<<"Cycle=" << local_cycle << ", Checking LOAD command "
        //          << load_cmd->get_seq_num() << " for issue" << std::endl;
        if(can_issue(load_cmd)) {
            load_cmd->update_status(COSIM_ISSUED);
            issue(load_cmd);
            load_queue.remove(load_cmd);
        }
    }

    if (!exe_queue.is_empty() && !exe_contlr.is_full()) {
        Command* exe_cmd = exe_queue.get_cmd_at(0);
        if (can_issue(exe_cmd)) {
            exe_cmd->update_status(COSIM_ISSUED);
            issue(exe_cmd);
            exe_queue.remove(exe_cmd);
        }
    }
}

void SAController::load(Command* cmd) {
    // fu_idle[cmd->get_op_type()] = false;
    max_load_dma_counter++;

    // Move-in (DRAM -> SPM/AccBuf) is modeled as a DMA read.
    // TODO: strided accesses, and we currently assume all data are accessed contiguously.
    dma_engine.submit(cmd, cmd->get_dram_addr(), cmd->get_rows() * cmd->get_cols() * DATA_SIZE, true,
                      local_cycle);
}

void SAController::store(Command* cmd) {
    // fu_idle[cmd->get_op_type()] = false;
    max_store_dma_counter++;

    // Move-out (SPM/AccBuf -> DRAM) is modeled as a DMA write.
    dma_engine.submit(cmd, cmd->get_dram_addr(), cmd->get_rows() * cmd->get_cols() * DATA_SIZE, false,
                      local_cycle);
}

void SAController::execute(Command* cmd) {
    tile_t T_M = cmd->get_T_M();
    tile_t T_K = cmd->get_T_K();
    tile_t T_N = cmd->get_T_N();
    tile_t _offset_to_MK = cmd->get_offset_to_MK();
    tile_t _offset_to_MN = cmd->get_offset_to_MN();
    tile_t _offset_to_KN = cmd->get_offset_to_KN();

    switch (cmd->get_op_type()) {
        case FAKE:
            cmd->set_done();
            complete(cmd->get_op_type());
            break;
        case FENCE:
        // std::cout<<"Cycle=" << local_cycle
        //           << ", Executing FENCE command " << cmd->get_seq_num()
        //           << std::endl;
            executing_fence_cmd = cmd;
            fu_idle[FENCE] = false;
            break;
        case CONFIG:
        // std::cout<<"Cycle=" << local_cycle
        //           << ", Executing CONFIG command " << cmd->get_seq_num()
        //           << std::endl;
            stonne->configMem(
                cmd->get_MK_address(),
                cmd->get_KN_address(),
                cmd->get_MN_address(),
                cmd->get_M(),
                cmd->get_N(),
                cmd->get_K(),
                cmd->get_T_M(),
                cmd->get_T_N(),
                cmd->get_T_K());

            cmd->set_done();
            complete(cmd->get_op_type());
            break;
        case PRELOAD:
            // std::cout << "Cycle=" << local_cycle
            //           << ", Executing PRELOAD command " << cmd->get_seq_num()
            //           << ", T_N: " << T_N << " T_K: " << T_K << std::endl;

            if (dataflow == DATAFLOW_OS) {
                Preload(T_M, T_N, _offset_to_MN, cmd);
            } else if (dataflow == DATAFLOW_WS) {
                Preload(T_N, T_K, _offset_to_KN, cmd);
            } else {
                assert(0);
            }

            fu_idle[PRELOAD] = false;
            set_preloading(true);
            executing_preload_cmds.push_back(cmd);
            break;
        case MATRIX_MULTIPLY:
        case MATRIX_MULTIPLY_FLIP: {
            bool flip = cmd->get_op_type() == MATRIX_MULTIPLY_FLIP;
            // std::cout << "Cycle=" << local_cycle
            //           << ", Executing MATRIX_MULTIPLY command "
            //           << cmd->get_seq_num() << (flip ? " (flip)" : "")
            //           << std::endl;
            MatrixMultiply(T_M, T_K, T_N, _offset_to_MK, _offset_to_MN,
                           _offset_to_KN, cmd, flip);
            fu_idle[cmd->get_op_type()] = false;
            executing_mm_cmds.push_back(cmd);
            performing_mm = true;
            break;
        }
        default:
            assert(0);
    }
}

void SAController::execute() {
    if (dataflow == DATAFLOW_WS && weights_preloaded()) {
        if (executing_preload_cmds.empty()) {
            // std::cerr << "Cycle=" << local_cycle
            //           << ", weights_preloaded but no tracking entry" << std::endl;
            this->msnet->reset_weights_preloaded();
        } else {
            Command* cmd = executing_preload_cmds.front();
            executing_preload_cmds.pop_front();
            // std::cout << "Cycle=" << local_cycle
            //           << ", complete weights loading " << cmd->get_seq_num()
            //           << std::endl;
            cmd->set_done();
            complete(cmd->get_op_type());
            set_preloading(false);

            this->msnet->reset_weights_preloaded();
        }
    }

    auto remove_tracked_mm_cmd = [this](Command* target) {
      assert(target);
      assert(!this->executing_mm_cmds.empty());

      if (this->executing_mm_cmds.front() == target) {
        this->executing_mm_cmds.pop_front();
      }
    };

    if (dataflow == DATAFLOW_WS && psums_accumulated()) {
            Command* cmd = this->wsAB->get_head_command();
            if (!cmd) {
                // std::cerr << "Cycle=" << local_cycle
                //                     << ", psums_accumulated but WSAccumulationBuffer has no head"
                //                     << std::endl;
            } else {
                // std::cout << "Cycle=" << local_cycle << ", complete psum accumulation "
                //                     << cmd->get_seq_num() << std::endl;

                cmd->set_done();
                complete(cmd->get_op_type());

                this->wsAB->reset_psums_accumulated();
                this->msnet->reset_fifos();
                remove_tracked_mm_cmd(cmd);
                performing_mm = false;
            }
    }

    if (dataflow == DATAFLOW_OS) {
        // std::cout << "size of executing_mm_cmds: " << executing_mm_cmds.size() << "\n";
        if (!executing_mm_cmds.empty()) {
            Command* cmd = executing_mm_cmds.front();

            // std::cout << "Cycle=" << local_cycle
            //           << ", complete psum accumulation (OS) "
            //           << cmd->get_seq_num()<<" stay="<<cmd->get_stay() << std::endl;

            if (cmd->get_stay()) {
                // std::cout << "        stay\n";
                if (this->msnet->is_compute_finished()) {
                    // std::cout << "Cycle=" << local_cycle
                    //           << ", complete psum accumulation (OS) stay"
                    //           << cmd->get_seq_num() << " stay=" << cmd->get_stay() << std::endl;

                    // if(cmd->acc_delay > 0) {
                    //     cmd->acc_delay--;
                    // } else {
                        cmd->set_done();
                        complete(cmd->get_op_type());

                        // this->msnet->reset_compute_finished();
                        executing_mm_cmds.pop_front();
                performing_mm = false;
                    // }
                }
            } else {
                if (psums_accumulated()) {
                    // std::cout << "       psums accumulated\n";
                    Command* head = this->wsAB->get_head_command();
                    assert(head);

                    if (head == cmd) {
                        // std::cout << "Cycle=" << local_cycle
                        //           << ", complete psum accumulation (OS) psums"
                        //           << cmd->get_seq_num() << " stay=" << cmd->get_stay() << std::endl;

                        // if(cmd->acc_delay > 0) {
                        //     cmd->acc_delay--;
                        // } else {
                            cmd->set_done();
                            complete(cmd->get_op_type());

                            this->wsAB->reset_psums_accumulated();

                            executing_mm_cmds.pop_front();
                performing_mm = false;
                        // }
                    }
                }
            }
        }
    }

    if (dataflow == DATAFLOW_OS && psums_received()) {
        Command* cmd = this->mem->get_command();
        if (!cmd) {
            // std::cerr << "Cycle=" << local_cycle
            //           << ", psums_received but OSMeshSDMemory head is empty"
            //           << std::endl;
        } else {
            // std::cout << "Cycle=" << local_cycle << ", complete psums loading "
            //           << cmd->get_seq_num() << std::endl;
            cmd->set_done();
            complete(cmd->get_op_type());
            this->msnet->reset_psums_received();
            this->mem->reset_compute_finished();

            Command *front = executing_preload_cmds.front();
            assert(front == cmd);
            executing_preload_cmds.pop_front();
        }
    }

    if (!exe_contlr.is_empty()) {
      Command* cmd = exe_contlr.get_cmd_at(0);
      if (can_execute(cmd)) {
        assert(cmd->is_ready());

        // if(cmd->spm_delay > 0) {
        //     cmd->spm_delay--;
        // } else {
            // std::cout<<"Cycle=" << local_cycle << ", Executing EXE command "
            //          << cmd->get_seq_num() << std::endl;
            cmd->update_status(COSIM_EXECUTING);
            execute(cmd);
            exe_contlr.remove(cmd);
        // }
      }
    }

    // TODO: LOAD and STORE execution
    if(!load_contlr.is_empty()) {
      Command* cmd = load_contlr.get_cmd_at(0);
      if (can_execute(cmd)) {
        assert(cmd->is_ready());
        // std::cout<<"Cycle=" << local_cycle << ", Executing LOAD command "
        //          << cmd->get_seq_num() << std::endl;
        cmd->update_status(COSIM_EXECUTING);
        load(cmd);
        load_contlr.remove(cmd);
      }
    }

    if(!store_contlr.is_empty()) {
      Command* cmd = store_contlr.get_cmd_at(0);
      if (can_execute(cmd)) {
        assert(cmd->is_ready());
        // std::cout<<"Cycle=" << local_cycle << ", Executing STORE command "
        //          << cmd->get_seq_num() << std::endl;
        cmd->update_status(COSIM_EXECUTING);
        store(cmd);
        store_contlr.remove(cmd);
      }
    }
}

void SAController::MatrixMultiply(unsigned int T_M,
                                  unsigned int T_K,
                                  unsigned int T_N,
                                  unsigned int _offset_to_MK,
                                  unsigned int _offset_to_MN,
                                  unsigned int _offset_to_KN,
                                  Command* cmd,
                                  bool flip) {
    this->mem->MatrixMultiply(T_M, T_K, T_N, _offset_to_MK, _offset_to_MN,
                              _offset_to_KN, cmd);
    if (flip) {
        this->msnet->MatrixMultiplyFlip(T_M, T_K, T_N);
    } else {
        if(cmd->get_stay()) {
            this->msnet->MatrixMultiplyStay(T_M, T_K, T_N);
        } else {
            this->msnet->MatrixMultiply(T_M, T_K, T_N);
        }
    }

    if (this->wsAB && !cmd->get_stay()) {
        this->wsAB->MatrixMultiply(T_M, T_K, T_N, _offset_to_MK, _offset_to_MN,
                                   _offset_to_KN, cmd);
    }
}

void SAController::Preload(unsigned int T_N,
                           unsigned int T_K,
                           unsigned int offset,
                           Command* cmd) {
    this->mem->preload(T_N, T_K, offset, cmd);
    this->msnet->preload();
}

void SAController::commit() {
    rob.commit();
}

void SAController::cycle() {

    if(executing_mm_cmds.size() >= 1) {
        exe_cycle++;
    } else {
        if(max_load_dma_counter > 0 && max_store_dma_counter > 0) {
            ld_st_cycle++;
        } else if(max_load_dma_counter > 0) {
            ld_cycle++;
        } else if(max_store_dma_counter > 0) {
            st_cycle++;
        }
    }

    local_cycle++;
    // 1) Progress DMA and retire completed commands
    dma_engine.cycle(local_cycle - 1);
    for (Command* done_cmd : dma_engine.take_completed()) {
        if (!done_cmd) {
            continue;
        }
        done_cmd->set_done();
        complete(done_cmd->get_op_type());
        // max_dma_counter--;
        switch (done_cmd->get_type()) {
            case LOAD:
            case LOAD_ACC:
                max_load_dma_counter--;
                break;
            case STORE:
                max_store_dma_counter--;
                break;
            default:
                assert(0);
        }
    }

    commit();
    execute();
    try_complete_fence();
    issue();
    dispatch();
}

bool SAController::can_release_fence() {
    if (!load_queue.is_empty() || !store_queue.is_empty() || !exe_queue.is_empty()) {
        // std::cout<<"size = "<<load_queue.get_used_capacity()<<","<<store_queue.get_used_capacity()<<","<<exe_queue.get_used_capacity()<<std::endl;
        return false;
    }

    if (!load_contlr.is_empty() || !store_contlr.is_empty() || !exe_contlr.is_empty()) {
        // std::cout<<"contlr size = "<<load_contlr.get_used_capacity()<<","<<store_contlr.get_used_capacity()<<","<<exe_contlr.get_used_capacity()<<std::endl;
        return false;
    }

    if (!executing_preload_cmds.empty()) {
        // std::cout<<"executing_preload_cmd queue not empty"<<std::endl;
        return false;
    }

    if (!executing_mm_cmds.empty()) {
        // std::cout<<"mm size= "<<executing_mm_cmds.size()<<std::endl;
        return false;
    }

    // DMA commands (MOVE_IN/MOVE_OUT/MOVE_IN_ACC) may still be in flight.
    if (dma_engine.has_inflight()) {
        return false;
    }

    return true;
}

void SAController::try_complete_fence() {
    if (!executing_fence_cmd) {
        return;
    }

    if (!can_release_fence()) {
        return;
    }

    if (executing_fence_cmd->get_op_type() == CONFIG) {
        // std::cout << "Cycle=" << local_cycle << ", complete Config command "
        //           << executing_fence_cmd->get_seq_num() << std::endl;

        // stonne->configMem(
        //     executing_fence_cmd->get_MK_address(),
        //     executing_fence_cmd->get_KN_address(),
        //     executing_fence_cmd->get_MN_address(),
        //     executing_fence_cmd->get_M(),
        //     executing_fence_cmd->get_N(),
        //     executing_fence_cmd->get_K(),
        //     executing_fence_cmd->get_T_M(),
        //     executing_fence_cmd->get_T_N(),
        //     executing_fence_cmd->get_T_K());

    } else {
        // std::cout << "Cycle=" << local_cycle << ", complete FENCE command "
        //           << executing_fence_cmd->get_seq_num() << std::endl;
    }

    executing_fence_cmd->set_done();
    complete(FENCE);
    executing_fence_cmd = nullptr;
    fence_active = false;
}

void ROB::commit() {
    int n = 0;
    while (!is_empty() && n <= commit_width) {
        Command* cmd = get_commit_head();
        if (cmd) {
            // std::cout << "Cycle=" << local_cycle << ", Committing command "
            //           << cmd->get_seq_num() << std::endl;
            cmd->update_status(COSIM_COMMITTED);
            pop_front();
            commit_seq_num = cmd->get_seq_num();
            cmd_commit = true;
            n++;
            cmd->set_dyn_inst_finished();

            // remove parent
            for (auto child : cmd->children) {
                child->remove_parent(cmd);
            }

            delete cmd;
        } else {
            break;
        }
    }

    local_cycle++;
}