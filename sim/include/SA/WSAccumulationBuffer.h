//
// Created by Francisco Munoz on 29/06/2020.
//

#ifndef __WSACCUMULATIONBUFFER__H__
#define __WSACCUMULATIONBUFFER__H__

#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <deque>
#include "Command.h"
#include "Config.h"
#include "Connection.h"
#include "Fifo.h"
#include "Tile.h"
#include "Unit.h"
#include "types.h"

class WSAccumulationBuffer : public Unit {
   private:
    unsigned int port_width;
    unsigned int n_write_ports;
    unsigned int ms_cols;
    unsigned int write_buffer_capacity;

    std::vector<Connection*> write_port_connections;
    address_t accbuf_address;
    Fifo*
        write_fifo;  // Fifo uses to store the writes before going to the memory
    cycles_t local_cycle = 0;
    unsigned int N;
    unsigned int M;
    unsigned int T_N;
    unsigned int T_M;

    DataflowType dataflow;

    struct PendingAccumulation {
        Command* cmd;
        unsigned int offset_to_MN;
        unsigned int expected_psums;
        unsigned int accumulated_psums;


        unsigned int expected_rows;
        unsigned int current_row;
    };

    std::deque<PendingAccumulation> pending_cmds;

   public:
    WSAccumulationBuffer(id_t id, std::string name, Config stonne_cfg);
    ~WSAccumulationBuffer();

    std::map<int, Connection*> getWriteConnections();

    // Cycle function
    void cycle();
    void receive();
    void accumulate();

    void MatrixMultiply(unsigned int T_M,
                        unsigned int T_K,
                        unsigned int T_N,
                        unsigned int _offset_to_MK,
                        unsigned int _offset_to_MN,
                        unsigned int _offset_to_KN,
                        Command* cmd);
    bool is_psums_accumulated() const;
    void reset_psums_accumulated();
    Command* get_head_command() const;

    void NPSumsConfiguration(unsigned int n_psums);
    void config(address_t MK_address, address_t KN_address,
                address_t output_address, unsigned int M, unsigned int N,
                unsigned int K, unsigned int T_M, unsigned int T_N,
                unsigned int T_K);
};

#endif
