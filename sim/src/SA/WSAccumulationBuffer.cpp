//
// Created by Francisco Munoz on 19/06/19.
//
#include "SA/WSAccumulationBuffer.h"
#include <assert.h>
#include "SA/utility.h"
#include <math.h>

// TODO Conectar los enlaces intermedios de forwarding
// This Constructor creates the reduction tree similar to the one shown in the paper
WSAccumulationBuffer::WSAccumulationBuffer(id_t id, std::string name, Config stonne_cfg) : Unit(id, name)
{
    // Collecting the parameters from configuration file
    this->port_width = stonne_cfg.m_SDMemoryCfg.port_width;
    this->n_write_ports = stonne_cfg.m_MSNetworkCfg.ms_cols;
    this->ms_cols = stonne_cfg.m_MSNetworkCfg.ms_cols;
    this->write_fifo = new Fifo(write_buffer_capacity);
    this->write_buffer_capacity = stonne_cfg.m_SDMemoryCfg.write_buffer_capacity;

    for (int i = 0; i < this->n_write_ports; i++)
    {
        Connection *input_connection = new Connection(this->port_width);
        write_port_connections.push_back(input_connection);
    }

    // this->accbuf_address = stonne_cfg.accbuf_address;
    // this->N = stonne_cfg.N;
    // this->T_M = stonne_cfg.T_M;
    // this->T_N = stonne_cfg.T_N;
    this->dataflow = stonne_cfg.dataflow;
}

WSAccumulationBuffer::~WSAccumulationBuffer()
{
}

void WSAccumulationBuffer::config(address_t MK_address, address_t KN_address,
                                  address_t output_address, unsigned int M,
                                  unsigned int N, unsigned int K, unsigned int T_M,
                                  unsigned int T_N, unsigned int T_K) {
  this->N = N;
  this->M = M;
  this->T_M = T_M;
  this->T_N = T_N;
  this->accbuf_address = output_address;
}

void WSAccumulationBuffer::receive()
{
    for (int i = 0; i < write_port_connections.size(); i++)
    { // For every write port
        if (write_port_connections[i]->existPendingData())
        {
            std::vector<DataPackage *> data_received = write_port_connections[i]->receive();
            for (int i = 0; i < data_received.size(); i++)
            {
                // std::cout << "WS AB receieve cycle=" << local_cycle << " num=" << data_received[i]->get_source() << " " << data_received[i]->get_data() << std::endl;
                write_fifo->push(data_received[i]);
            }
        }
    }
}

void WSAccumulationBuffer::accumulate() {
    if (!write_fifo->isEmpty()) {
        int size_of_fifo = write_fifo->size();
        PendingAccumulation& current = pending_cmds.front();
        for (int i = 0; i < size_of_fifo; i++) {
            assert(!pending_cmds.empty());

            DataPackage* pck_received = write_fifo->pop();

            int source = pck_received->get_source() % this->ms_cols;

            int row = current.current_row;
            if (dataflow == DATAFLOW_OS) {
                row = pck_received->get_source() / this->ms_cols;
            }

            int index = current.offset_to_MN + row * N + source;

            if (index < this->M * this->N) {
                // std::cout
                //     << "accumulate i = " << i << " cycle=" << local_cycle
                //     << " num=" << source << " " << pck_received->get_data() << " ori="
                //     << this->accbuf_address[current.offset_to_MN + row * N + source]
                //     << std::endl;
                // std::cout << "offset_to_MN=" << current.offset_to_MN << " row=" << row
                //           << " N=" << N << " source=" << source
                //           << " index=" << current.offset_to_MN + row * N + source
                //           << std::endl;
                // this->accbuf_address[current.offset_to_MN + row * N + source] +=
                //     pck_received->get_data();
            }

            current.accumulated_psums++;
            // std::cout << "current_output_iteration=" << current.accumulated_psums
            //           << std::endl;
            delete pck_received;
        }

        current.current_row++;
    }
}

void WSAccumulationBuffer::cycle()
{
    local_cycle++;
    receive();
    accumulate();
}

std::map<int, Connection *> WSAccumulationBuffer::getWriteConnections()
{
    std::map<int, Connection *> connection_map;
    for (int i = 0; i < write_port_connections.size(); i++)
    {
        connection_map[i] = write_port_connections[i];
    }
    return connection_map;
}

void WSAccumulationBuffer::MatrixMultiply(unsigned int T_M, unsigned int T_K, unsigned int T_N, unsigned int _offset_to_MK, unsigned int _offset_to_MN, unsigned int _offset_to_KN, Command *cmd)
{
    PendingAccumulation pending;
    pending.cmd = cmd;
    pending.offset_to_MN = _offset_to_MN;
    if (this->dataflow == DATAFLOW_WS) {
        pending.expected_psums = T_M * T_N;
        // pending.expected_psums = this->T_M * this->T_N;
    } else if (this->dataflow == DATAFLOW_OS) {
        pending.expected_psums = this->T_M * this->T_N;
    }
    pending.accumulated_psums = 0;
    pending.current_row = 0;
    pending.expected_rows = T_N;
    pending_cmds.push_back(pending);
}

void WSAccumulationBuffer::NPSumsConfiguration(unsigned int n_psums) {
    //NOTHING TO DO
}

bool WSAccumulationBuffer::is_psums_accumulated() const {
    if (pending_cmds.empty()) {
        return false;
    }
    const PendingAccumulation &current = pending_cmds.front();
    return current.accumulated_psums >= current.expected_psums;
}

void WSAccumulationBuffer::reset_psums_accumulated() {
    if (!pending_cmds.empty() &&
        pending_cmds.front().accumulated_psums >= pending_cmds.front().expected_psums) {
        pending_cmds.pop_front();
    }
}

Command* WSAccumulationBuffer::get_head_command() const {
    if (pending_cmds.empty()) {
        return nullptr;
    }
    return pending_cmds.front().cmd;
}