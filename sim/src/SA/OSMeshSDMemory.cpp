
#include "SA/OSMeshSDMemory.h"
#include <assert.h>
#include <iostream>
#include "SA/utility.h"

OSMeshSDMemory::OSMeshSDMemory(id_t id, std::string name, Config stonne_cfg, Connection *write_connection) : MemoryController(id, name)
{
    this->write_connection = write_connection;
    // Collecting parameters from the configuration file
    this->ms_rows = stonne_cfg.m_MSNetworkCfg.ms_rows; // Used to send data
    this->ms_cols = stonne_cfg.m_MSNetworkCfg.ms_cols;
    this->n_read_ports = stonne_cfg.m_SDMemoryCfg.n_read_ports;
    this->n_write_ports = stonne_cfg.m_SDMemoryCfg.n_write_ports;
    this->write_buffer_capacity = stonne_cfg.m_SDMemoryCfg.write_buffer_capacity;
    this->port_width = stonne_cfg.m_SDMemoryCfg.port_width;
    // End collecting parameters from the configuration file
    // Initializing parameters
    this->ms_size_per_input_port = (this->ms_rows + this->ms_cols) / this->n_read_ports;
    this->write_fifo = new Fifo(write_buffer_capacity);
    for (int i = 0; i < this->n_read_ports; i++)
    {
        Fifo *read_fi = new Fifo(this->write_buffer_capacity);
        Fifo *psum_fi = new Fifo(this->write_buffer_capacity);
        input_fifos.push_back(read_fi);
        psum_fifos.push_back(psum_fi);
        this->sdmemoryStats.n_SRAM_read_ports_weights_use.push_back(0); // To track information
        this->sdmemoryStats.n_SRAM_read_ports_inputs_use.push_back(0);  // To track information
        this->sdmemoryStats.n_SRAM_read_ports_psums_use.push_back(0);   // To track information
    }
    for (int i = 0; i < this->n_write_ports; i++)
    {                                                            // To track information
        this->sdmemoryStats.n_SRAM_write_ports_use.push_back(0); // To track information
    } // To track information
    this->configuration_done = false;
    this->execution_finished = false;
    this->metadata_loaded = false;
    this->layer_loaded = false;
    this->region_loaded = false;
    this->local_cycle = 0;
    this->current_state = OS_IDLE;
    this->current_output = 0;
    this->output_size = 0;
    this->current_output_iteration = 0;
    this->output_size_iteration = 0;
    this->current_M = 0;
    this->current_N = 0;
    this->current_K = 0;
    this->iteration_completed = false;
    this->n_iterations_completed = 0;
    pending_cmds.clear();
}

OSMeshSDMemory::~OSMeshSDMemory()
{
    delete write_fifo;
    // Deleting the input ports
    for (int i = 0; i < this->n_read_ports; i++)
    {
        delete input_fifos[i];
        delete psum_fifos[i];
    }
}

void OSMeshSDMemory::setWriteConnections(std::vector<Connection *> write_port_connections)
{
    this->write_port_connections = write_port_connections; // Copying all the poiners
    // assert(this->write_port_connections.size()==this->n_write_ports);
}

void OSMeshSDMemory::setReadConnections(std::vector<Connection *> read_connections)
{
    assert(read_connections.size() == n_read_ports); // Checking that the number of input ports is valid.
    this->read_connections = read_connections;       // Copying all the pointers
}

void OSMeshSDMemory::preload(unsigned int T_M, unsigned int T_N, unsigned int offset, Command *cmd)
{
    PendingRequest req;
    req.type = REQ_PRELOAD;
    req.rows_used = T_M;
    req.cols_used = T_N;
    req.offset_to_MN = offset;
    req.current_M = Padded_T_M - 1;
    req.cmd = cmd;
    pending_requests.push_back(req);

    // if (current_state == OS_IDLE || current_state == OS_CONFIGURING) {
    //     process_next_request();
    // }
}

void OSMeshSDMemory::process_next_request() {
    if (pending_requests.empty()) {
        return;
    }

    PendingRequest &req = pending_requests.front();
    if (req.type == REQ_PRELOAD) {
        current_state = OS_DIST_PSUMS;
        this->rows_used = req.rows_used;
        this->cols_used = req.cols_used;
        this->offset_to_MN = req.offset_to_MN;
        this->current_M = req.current_M;
        pending_cmds.push_back(req.cmd); 
    } else if (req.type == REQ_MM) {
        current_state = OS_DIST_INPUTS;
        
        offset_to_MK = req.offset_to_MK;
        offset_to_MN = req.offset_to_MN;
        offset_to_KN = req.offset_to_KN;

        this->rows_used = req.rows_used;
        this->cols_used = req.cols_used;
        this->k_used = req.k_used;

        this->current_K = req.current_K;
        this->current_M = req.current_M;
        this->T_M = req.T_M;
        this->T_N = req.T_N;
        this->T_K = req.T_K;
    }
}

void OSMeshSDMemory::MatrixMultiply(unsigned int T_M, unsigned int T_K, unsigned int T_N, unsigned int _offset_to_MK, unsigned int _offset_to_MN, unsigned int _offset_to_KN, Command *cmd)
{
    // assert((_offset_to_MK % T_M == 0) && (_offset_to_MK % T_K == 0));

    PendingRequest req;
    req.type = REQ_MM;
    req.offset_to_MK = _offset_to_MK;
    req.offset_to_MN = _offset_to_MN;
    req.offset_to_KN = _offset_to_KN;
    req.rows_used = T_M;
    req.cols_used = T_N;
    req.k_used = T_K;
    req.current_K = 0;
    req.current_M = 0;
    req.T_M = T_M;
    req.T_N = T_N;
    req.T_K = T_K;
    req.cmd = cmd;
    pending_requests.push_back(req);

    // If the memory is idle (or will become idle), start the next MM as soon as possible.
    // if (current_state == OS_IDLE || current_state == OS_CONFIGURING) {
    //     process_next_request();
    // }
}


void OSMeshSDMemory::cycle()
{
    // Sending input data over read_connection
    std::vector<DataPackage *> data_to_send; // Input and weight temporal storage
    // std::vector<DataPackage*> psum_to_send; // psum temporal storage
    this->local_cycle += 1;
    this->sdmemoryStats.total_cycles++; // To track information

    if(current_state == OS_IDLE) {
        process_next_request();
    }

    if (current_state == OS_DIST_PSUMS) {
        if (this->current_M > this->T_M - 1) {
            for (int i = 0; i < this->ms_cols; i++) {
                data_t data = 0;
                DataPackage* pck_to_send = new DataPackage(sizeof(data_t), data, PSUM, 0, UNICAST, i);
                // std::cout << "??? index=" << -1 << " offset_to_MN: " << offset_to_MN << " current_M: " << this->current_M << " data:" << data << std::endl;
                this->sendPackageToInputFifos(pck_to_send);
            }
        } else {
            for (int i = 0; i < this->ms_cols; i++) {
                data_t data = 0;

                if (i < this->cols_used) {
                    // data = this->output_address[offset_to_MN + current_M * N + i];
                    // this->output_address[offset_to_MN + current_M * N + i] = 0;
                }
                // std::cout << "??? index=" << offset_to_MN + current_M * N + i << " offset_to_MN: " << offset_to_MN << " current_M: " << this->current_M << " data:" << data << std::endl;
                DataPackage* pck_to_send = new DataPackage(sizeof(data_t), data, PSUM, 0, UNICAST, i);
                this->sendPackageToInputFifos(pck_to_send);
            }
        }

        if(this->current_M == 0) {
            current_state = OS_IDLE;

            if (!pending_requests.empty()) {
                pending_requests.pop_front();
            }

            // if (!pending_requests.empty()) {
            //     process_next_request();
            // }
        } else {
            this->current_M--;
        }
    }

    if (current_state == OS_DIST_INPUTS) {
        // Distribution of the stationary matrix
        unsigned int dest = 0;  // MS destination


        // std::cout<<" this TK="<<this->T_K<<"\n";

        if (current_K <= this->T_K - 1) {
            // SENDING N PACKAGES
            for (int i = 0; i < this->ms_cols; i++) {
                data_t data = 0;  // Accessing to memory

                if (i < this->cols_used) {
                    // std::cout << "offset_to_KN: " << offset_to_KN << " i: " << i
                    //           << " K:" << K << " current_K: " << current_K
                    //           << " K:" << K << " N:" << N << "\n";
                    assert(offset_to_KN + i * K + current_K < N * K);
                    // data = this->KN_address[offset_to_KN + i * K + current_K];  // Notice that in dense operation the KN matrix is actually NK4
                }
                // std::cout <<"cycle=" << local_cycle << " offset_to_KN: " << offset_to_KN << " i: " << i << " k:" << K << " current_K: " << current_K << " data:" << data << std::endl;

                sdmemoryStats.n_SRAM_weight_reads++;
                DataPackage* pck_to_send = new DataPackage(sizeof(data_t), data, WEIGHT, 0, UNICAST, i);
                this->sendPackageToInputFifos(pck_to_send);
            }

            // SENDING M PACKAGES. Note that not delay is necessary as the fifos will carry out these delays
            for (int i = 0; i < this->ms_rows; i++) {
                data_t data = 0;  // Accessing to memory

                if (i < this->rows_used) {
                    assert(offset_to_MK + i * K + current_M < M * K);
                    // data = this->MK_address[offset_to_MK + i * K + current_M];
                }
                // std::cout <<"cycle=" << local_cycle << " offset_to_MK: " << offset_to_MK << " i: " << i << " k:" << K << " current_M: " << current_M << " data:" << data << std::endl;

                sdmemoryStats.n_SRAM_input_reads++;
                DataPackage* pck_to_send = new DataPackage(sizeof(data_t), data, IACTIVATION, 0, UNICAST, i + this->ms_cols);
                this->sendPackageToInputFifos(pck_to_send);
            }
        } else {
            // Sending padding zeros
            for (int i = 0; i < this->ms_cols; i++) {
                data_t data = 0;
                DataPackage* pck_to_send = new DataPackage(sizeof(data_t), data, WEIGHT, 0, UNICAST, i);
                // std::cout << "index=" << -1 << " offset_to_KN: " << offset_to_KN << " current_K: " << current_K << " data:" << data << std::endl;
                this->sendPackageToInputFifos(pck_to_send);
            }

            for (int i = 0; i < this->ms_rows; i++) {
                data_t data = 0;
                DataPackage* pck_to_send = new DataPackage(sizeof(data_t), data, IACTIVATION, 0, UNICAST, i + this->ms_cols);
                // std::cout << "index=" << -1 << " offset_to_MK: " << offset_to_MK << " current_M: " << current_M << " data:" << data << std::endl;
                this->sendPackageToInputFifos(pck_to_send);
            }
        }

        current_K++;
        current_M++;

        if (current_K == Padded_T_K) {
            assert(current_M == Padded_T_K);
            current_state = OS_IDLE;

            if (!pending_requests.empty()) {
                pending_requests.pop_front();
            }

            // if (!pending_requests.empty()) {
            //     process_next_request();
            // }
        }
    }  // END STATE

    this->send();
}

bool OSMeshSDMemory::isExecutionFinished()
{
    return this->execution_finished;
}

/* The traffic generation algorithm generates a package that contains a destination for all the ms. We have to divide it into smaller groups of ms since they are divided into several ports */
void OSMeshSDMemory::sendPackageToInputFifos(DataPackage *pck)
{
    // BROADCAST PACKAGE
    if (pck->isBroadcast())
    {
        // Send to all the ports with the flag broadcast enabled
        for (int i = 0; i < this->n_read_ports; i++)
        {
            // Creating a replica of the package to be sent to each port
            DataPackage *pck_new = new DataPackage(pck->get_size_package(), pck->get_data(), pck->get_data_type(), i, BROADCAST); // Size, data, data_type, source (port in this case), BROADCAST
            // Sending the replica to the suitable fifo that correspond with the port
            if (pck->get_data_type() == PSUM)
            { // Actually a PSUM cannot be broadcast. But we put this for compatibility
                psum_fifos[i]->push(pck_new);
            }
            else
            { // INPUT OR WEIGHT
                // Seting iteration of the package
                pck_new->setIterationK(pck->getIterationK()); // Used to avoid sending packages from a certain iteration without performing the previous.
                input_fifos[i]->push(pck_new);
            }
        }
    }

    // UNICAST PACKAGE
    else if (pck->isUnicast())
    {
        // We only have to send the weight to one port and change the destination to adapt it to the subgroup
        unsigned int dest = pck->get_unicast_dest(); // This is according to ALL the mswitches.
        unsigned int input_port = dest / this->ms_size_per_input_port;
        unsigned int local_dest = dest % this->ms_size_per_input_port;
        // Creating the package
        DataPackage *pck_new = new DataPackage(pck->get_size_package(), pck->get_data(), pck->get_data_type(), input_port, UNICAST, local_dest); // size, data, type, source (port), UNICAST, dest_local
        // Sending to the fifo corresponding with port input_port
        if (pck->get_data_type() == PSUM)
        { // Actually a PSUM cannot be broadcast. But we put this for compatibility
            psum_fifos[input_port]->push(pck_new);
        }
        else
        { // INPUT OR WEIGHT
            input_fifos[input_port]->push(pck_new);
            pck_new->setIterationK(pck->getIterationK());
        }
    }

    // MULTICAST PACKAGE
    else
    {                                        // The package is multicast and then we have to send the package to several ports
        const bool *dest = pck->get_dests(); // One position for mswitch in all the msarray
        bool thereis_receiver;
        for (int i = 0; i < this->n_read_ports; i++)
        { // Checking each port with size this->ms_size_per_input_port each. Total=ms_size
            unsigned int port_index = i * this->ms_size_per_input_port;
            thereis_receiver = false;                                  // To know at the end if the group
            bool *local_dest = new bool[this->ms_size_per_input_port]; // Local destination array for the subtree corresponding with the port i
            for (int j = 0; j < this->ms_size_per_input_port; j++)
            {                                         // For each ms in the group of the port i
                local_dest[j] = dest[port_index + j]; // Copying the subarray
                if (local_dest[j] == true)
                {
                    thereis_receiver = true; // To avoid iterating again to know whether the data have to be sent to the port or not.
                }
            }

            if (thereis_receiver)
            { // If this port have at least one ms to true then we send the data to this port i
                DataPackage *pck_new = new DataPackage(pck->get_size_package(), pck->get_data(), pck->get_data_type(), i, MULTICAST, local_dest, this->ms_size_per_input_port);
                if (pck->get_data_type() == PSUM)
                {
                    psum_fifos[i]->push(pck_new);
                }

                else
                {
                    pck_new->setIterationK(pck->getIterationK());
                    input_fifos[i]->push(pck_new);
                }
            }
            else
            {
                delete[] local_dest; // If this vector is not sent we remove it.
            }
        }
    }

    delete pck; // We have created replicas of the package for the ports needed so we can delete this
}

void OSMeshSDMemory::send()
{
    // Iterating over each port and if there is data in its fifo we send it. We give priority to the psums

    for (int i = 0; i < this->n_read_ports; i++)
    {
        std::vector<DataPackage *> pck_to_send;
        if (!this->psum_fifos[i]->isEmpty())
        { // If there is something we may send data though the connection
            DataPackage *pck = psum_fifos[i]->pop();
#ifdef DEBUG_MEM_INPUT
            std::cout << "[MEM_INPUT] Cycle " << local_cycle << ", Sending a psum through input port " << i << std::endl;
#endif
            pck_to_send.push_back(pck);
            this->sdmemoryStats.n_SRAM_read_ports_psums_use[i]++; // To track information
            // Sending to the connection
            this->read_connections[i]->send(pck_to_send);
        }
        // If psums fifo is empty then input fifo is checked. If psum is not empty then else do not compute. Important this ELSE to give priority to the psums and do not send more than 1 pck
        else if (!this->input_fifos[i]->isEmpty())
        {
            // If the package belongs to a certain k iteration but the previous k-1 iteration has not finished the package is not sent
            DataPackage *pck = input_fifos[i]->front(); // Front because we are not sure if we have to send it.

            if (pck->get_data_type() == WEIGHT)
            {
                this->sdmemoryStats.n_SRAM_read_ports_weights_use[i]++; // To track information
#ifdef DEBUG_MEM_INPUT
                std::cout << "[MEM_INPUT] Cycle " << local_cycle << ", Sending a WEIGHT through input port " << i << std::endl;
#endif
            }
            else
            {
                this->sdmemoryStats.n_SRAM_read_ports_inputs_use[i]++; // To track information
#ifdef DEBUG_MEM_INPUT
                std::cout << "[MEM_INPUT] Cycle " << local_cycle << ", Sending an INPUT ACTIVATION through input port " << i << std::endl;
#endif
            }
            pck_to_send.push_back(pck);                   // storing into the vector data type structure used in class Connection
            this->read_connections[i]->send(pck_to_send); // Sending the input or weight through the connection
            input_fifos[i]->pop();                        // pulling from fifo
        }
    }
}

// TODO Remove this connection
void OSMeshSDMemory::receive()
{ // TODO control if there is no space in queue
    if (this->write_connection->existPendingData())
    {
        std::vector<DataPackage *> data_received = write_connection->receive();
        for (int i = 0; i < data_received.size(); i++)
        {
            write_fifo->push(data_received[i]);
        }
    }
    for (int i = 0; i < write_port_connections.size(); i++)
    { // For every write port
        if (write_port_connections[i]->existPendingData())
        {
            std::vector<DataPackage *> data_received = write_port_connections[i]->receive();
            for (int i = 0; i < data_received.size(); i++)
            {
                write_fifo->push(data_received[i]);
            }
        }
    }
}

void OSMeshSDMemory::printStats(std::ofstream &out, unsigned int indent)
{
    out << ind(indent) << "\"SDMemoryStats\" : {" << std::endl; // TODO put ID
    this->sdmemoryStats.print(out, indent + IND_SIZE);
    out << ind(indent) << "}"; // Take care. Do not print endl here. This is parent responsability
}

void OSMeshSDMemory::printEnergy(std::ofstream &out, unsigned int indent)
{
    /*
        This component prints:
            - The number of SRAM reads
            - The number of SRAM writes

        Note that the number of times that each port is used is not shown. This is so because the use of those wires are
        taken into account in the CollectionBus and in the DSNetworkTop
   */

    counter_t reads = this->sdmemoryStats.n_SRAM_weight_reads + this->sdmemoryStats.n_SRAM_input_reads + this->sdmemoryStats.n_SRAM_psum_reads;
    counter_t writes = this->sdmemoryStats.n_SRAM_psum_writes;
    out << ind(indent) << "GLOBALBUFFER READ=" << reads; // Same line
    out << ind(indent) << " WRITE=" << writes << std::endl;
}

void OSMeshSDMemory::reset() {

    region = nullptr;
    this->sdmemoryStats.reset();

    for (int i = 0; i < this->n_read_ports; i++)
    {
        this->sdmemoryStats.n_SRAM_read_ports_weights_use.push_back(0);
        this->sdmemoryStats.n_SRAM_read_ports_inputs_use.push_back(0);
        this->sdmemoryStats.n_SRAM_read_ports_psums_use.push_back(0);  
    }
    for (int i = 0; i < this->n_write_ports; i++)
    {                                                          
        this->sdmemoryStats.n_SRAM_write_ports_use.push_back(0);
    }

    this->configuration_done = false;
    this->execution_finished = false;
    this->metadata_loaded = false;
    this->layer_loaded = false;
    this->region_loaded = false;
    this->local_cycle = 0;
    this->current_state = OS_IDLE;
    this->current_output = 0;
    this->output_size = 0;
    this->current_output_iteration = 0;
    this->output_size_iteration = 0;
    this->current_M = 0;
    this->current_N = 0;
    this->current_K = 0;
    this->iteration_completed = false;
    this->n_iterations_completed = 0;

    pending_requests.clear();
}

Command* OSMeshSDMemory::get_command() {
    if (pending_cmds.empty()) {
        return nullptr;
    }
    return pending_cmds.front();
}

bool OSMeshSDMemory::is_idle() {
    return this->current_state == OS_IDLE;
}

void OSMeshSDMemory::reset_compute_finished() {
    // execution_finished = false;
    // current_output_iteration = 0;
    if (!pending_cmds.empty()) {
        pending_cmds.pop_front();
    }
}

void OSMeshSDMemory::config(address_t MK_address, address_t KN_address,
                            address_t output_address, unsigned int M,
                            unsigned int N, unsigned int K, unsigned int T_M,
                            unsigned int T_N, unsigned int T_K)
{
    this->MK_address = MK_address;
    this->KN_address = KN_address;
    this->output_address = output_address;
    this->M = M;
    this->N = N;
    this->K = K;

    this->Padded_T_M = T_M;
    this->Padded_T_N = T_N;
    this->Padded_T_K = T_K;
}