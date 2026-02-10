// Created by Francisco Munoz-Martinez on 13/06/2019

#include "SA/MultiplierWS.h"
#include <assert.h>
#include "SA/utility.h"
/*
 */

MultiplierWS::MultiplierWS(id_t id, std::string name, int row_num, int col_num, Config stonne_cfg) : Unit(id, name)
{
    this->row_num = row_num;
    this->col_num = col_num;
    // Extracting parameters from configuration file
    this->latency = stonne_cfg.m_MSwitchCfg.latency;
    this->input_ports = stonne_cfg.m_MSwitchCfg.input_ports;
    this->output_ports = stonne_cfg.m_MSwitchCfg.output_ports;
    this->forwarding_ports = stonne_cfg.m_MSwitchCfg.forwarding_ports;
    this->buffers_capacity = stonne_cfg.m_MSwitchCfg.buffers_capacity;
    this->port_width = stonne_cfg.m_MSwitchCfg.port_width;
    this->ms_rows = stonne_cfg.m_MSNetworkCfg.ms_rows;
    this->ms_cols = stonne_cfg.m_MSNetworkCfg.ms_cols;
    // End of extracting parameters from the configuration file

    // Parameters initialization
    // this->top_weight_fifo = new SyncFifo(buffers_capacity);
    this->top_psum_fifo = new SyncFifo(buffers_capacity);
    this->bottom_weight_fifo = new SyncFifo(buffers_capacity);
    this->bottom_psum_fifo = new SyncFifo(buffers_capacity);
    this->right_fifo = new SyncFifo(buffers_capacity);
    // End parameters initialization

    unsigned int left_cnt = 1;
    unsigned int acc_cnt = 1;
    unsigned int top_cnt = 1;

    if (row_num == ms_rows - 1)
    {
        acc_cnt = ms_cols - col_num;
    }

    if (col_num == 0)
    {
        left_cnt = row_num + 1;
    }

    if (row_num == 0) {
        top_cnt = col_num + 1;
    }

    // std::cout << "MultiplierWS (" << row_num << ", " << col_num << ") sync fifo left cnt: " << left_cnt<<" acc cnt:"<<acc_cnt << std::endl;

    this->top_weight_fifo = new SyncFifo(buffers_capacity, top_cnt);
    this->left_fifo = new SyncFifo(buffers_capacity, left_cnt);
    this->accbuffer_fifo = new SyncFifo(buffers_capacity, acc_cnt);

    this->local_cycle = 0;
    this->forward_right = false;  // Based on rows (windows) left and dimensions
    this->forward_bottom = false; // Based on columns (filters) left and dimensions
    this->VN = 0;
    this->num = this->row_num * this->ms_cols + this->col_num; // Multiplier ID, used just for information

    current_state = WS_CONFIGURING; // Initial state
    this->weight1 = 0;
    this->weight2 = 0;
    this->weight_limit = this->ms_rows - this->row_num;
    this->weight_count = 0;
    this->prop = false;
    this->activation_limit = 1;
}

MultiplierWS::MultiplierWS(id_t id, std::string name, int row_num, int col_num, Config stonne_cfg, Connection *left_connection, Connection *right_connection, Connection *top_connection, Connection *bottom_connection) : MultiplierWS(id, name, row_num, col_num, stonne_cfg)
{
    this->setLeftConnection(left_connection);
    this->setRightConnection(right_connection);
    this->setTopConnection(top_connection);
    this->setBottomConnection(bottom_connection);
}

MultiplierWS::~MultiplierWS()
{
    delete this->left_fifo;
    delete this->right_fifo;
    delete this->top_weight_fifo;
    delete this->top_psum_fifo;
    delete this->bottom_weight_fifo;
    delete this->bottom_psum_fifo;
}

void MultiplierWS::setPropSignal(bool prop) {
    this->prop = prop;
}

void MultiplierWS::setActivationLimit(unsigned int limit) {
    this->activation_limit = limit;
}

void MultiplierWS::preload() {
    this->weight_limit = this->ms_rows - this->row_num;
    this->weight_count = 0;

    // std::cout << "[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Preload: prop set to true (load weight1)" << std::endl;
    flip_ready = false;
}

void MultiplierWS::MatrixMultiply(tile_t T_M, tile_t T_K, tile_t T_N) {

    if (act_limit_queue.empty() || act_limit_queue.back().T_M != T_M) {
        ActLimit limit;
        limit.cycle = this->local_cycle + row_num + col_num;
        limit.T_K = T_K;
        limit.T_M = T_M;
        limit.T_N = T_N;
        act_limit_queue.push_back(limit);
    }

    // std::cout << "[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") MatrixMultiply" << std::endl;
}

void MultiplierWS::MatrixMultiplyFlip(tile_t T_M, tile_t T_K, tile_t T_N) {
    flip_counter = row_num + col_num;

    if (act_limit_queue.empty() || act_limit_queue.back().T_M != T_M) {
        ActLimit limit;
        limit.cycle = this->local_cycle + row_num + col_num;
        limit.T_K = T_K;
        limit.T_M = T_M;
        limit.T_N = T_N;
        act_limit_queue.push_back(limit);
    }

    // std::cout << "[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") MatrixMultiplyFlip" << std::endl;
}

void MultiplierWS::resetSignals()
{

    this->forward_right = false;
    this->forward_bottom = false;
    this->VN = 0;
    this->prop = false;
    this->weight_limit = this->ms_rows - this->row_num;
    this->weight_count = 0;
    flip_ready = false;
    act_limit_queue.clear();

    while (!accbuffer_fifo->isEmpty())
    {
        accbuffer_fifo->pop();
    }

    while (!left_fifo->isEmpty())
    {
        left_fifo->pop();
    }

    while (!right_fifo->isEmpty())
    {
        right_fifo->pop();
    }

    while (!top_weight_fifo->isEmpty())
    {
        top_weight_fifo->pop();
    }

    while (!top_psum_fifo->isEmpty())
    {
        top_psum_fifo->pop();
    }

    while (!bottom_weight_fifo->isEmpty())
    {
        bottom_weight_fifo->pop();
    }

    while (!bottom_psum_fifo->isEmpty())
    {
        bottom_psum_fifo->pop();
    }
}
void MultiplierWS::setLeftConnection(Connection *left_connection)
{
    this->left_connection = left_connection;
}
void MultiplierWS::setRightConnection(Connection *right_connection)
{ // Set the right connection of the ms
    this->right_connection = right_connection;
}

void MultiplierWS::setTopConnection(Connection *top_connection)
{
    this->top_connection = top_connection;
}

void MultiplierWS::setBottomConnection(Connection *bottom_connection)
{ // Set the input connection of the switch
    this->bottom_connection = bottom_connection;
}

void MultiplierWS::setAccBufferConnection(Connection *accbuffer_connection)
{ // Set the output connection
    this->accbuffer_connection = accbuffer_connection;
}

void MultiplierWS::configureBottomSignal(bool bottom_signal)
{
    this->forward_bottom = bottom_signal;
}

void MultiplierWS::configureRightSignal(bool right_signal)
{
    this->forward_right = right_signal;
}

void MultiplierWS::setVirtualNeuron(unsigned int VN)
{
    this->VN = VN;
    this->mswitchStats.n_configurations++;
#ifdef DEBUG_MSWITCH_CONFIG
    std::cout << "[MSWITCH_CONFIG] MultiplierWS " << ". VirtualNeuron: " << this->VN << std::endl;
#endif
}

void MultiplierWS::send()
{ // Send the result through the outputConnection
    // Sending weights
    std::vector<DataPackage *> vector_to_send_psums_or_weights;
    if (!bottom_weight_fifo->isEmpty())
    {
        DataPackage *data_to_send = bottom_weight_fifo->pop();
#ifdef DEBUG_MSWITCH_FUNC
        std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " has forward a weight to the bottom connection" << std::endl;
#endif
        vector_to_send_psums_or_weights.push_back(data_to_send);
    }

    if (!bottom_psum_fifo->isEmpty())
    {
        DataPackage *data_to_send = bottom_psum_fifo->pop();
#ifdef DEBUG_MSWITCH_FUNC
        std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " has forward a psum to the bottom connection" << std::endl;
#endif
        vector_to_send_psums_or_weights.push_back(data_to_send);
    }

    // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " Sending to bottom connection: "<<vector_to_send_psums_or_weights.size()<<" packages."<<std::endl;

    if (vector_to_send_psums_or_weights.size() > 0)
    {
        this->mswitchStats.n_bottom_forwardings_send += vector_to_send_psums_or_weights.size();
        this->bottom_connection->send(vector_to_send_psums_or_weights);
    }

    // Sending activations
    std::vector<DataPackage *> vector_to_send_activations;
    if (!right_fifo->isEmpty())
    {
        DataPackage *data_to_send = right_fifo->pop();
#ifdef DEBUG_MSWITCH_FUNC
        std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " has forward a data to the right connection" << std::endl;
#endif
        vector_to_send_activations.push_back(data_to_send);
    }
    if (vector_to_send_activations.size() > 0)
    {
        this->mswitchStats.n_right_forwardings_send++;
        this->right_connection->send(vector_to_send_activations);
    }

    // Sending psums to the accumulation buffer
    std::vector<DataPackage *> vector_to_send_psums;
    // std::cout<<"acc buffer size: "<<accbuffer_fifo->size()<<"\n";
    if (!accbuffer_fifo->isEmpty())
    {
        DataPackage *data_to_send = accbuffer_fifo->pop();

        if (data_to_send) {
            // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " has forward a data to the accumulation buffer" << " addr=" << this->accbuffer_connection << " data=" << data_to_send->get_data() << std::endl;
            vector_to_send_psums.push_back(data_to_send);
        }
    }
    if (vector_to_send_psums.size() > 0)
    {
        this->accbuffer_connection->send(vector_to_send_psums);
    }
}

void MultiplierWS::receive()
{ // Receive a package either from the left or the top connection

    if (left_connection->existPendingData())
    {
        std::vector<DataPackage *> data_received = left_connection->receive();
        assert(data_received.size() == 1);
        for (int i = 0; i < data_received.size(); i++)
        {
            DataPackage *pck = data_received[i];
            assert(pck->get_data_type() == IACTIVATION);
            // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " has received an IACTIVATION from the left connection" << std::endl;
            this->mswitchStats.n_left_forwardings_receive++;

            left_fifo->push(pck);
        }
    }

    if (top_connection->existPendingData())
    {
        std::vector<DataPackage *> data_received = top_connection->receive();
        // assert(data_received.size() == 1); // Can be more than 1 now
        for (int i = 0; i < data_received.size(); i++)
        {
            DataPackage *pck = data_received[i];

            assert(pck->get_data_type() == WEIGHT || pck->get_data_type() == COMPUTED_PSUM);
            // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " has received a weight/psum from the top connection" << std::endl;
            this->mswitchStats.n_top_forwardings_receive++;

            if (pck->get_data_type() == WEIGHT) {
                top_weight_fifo->push(pck);
            } else {
                top_psum_fifo->push(pck);
            }
        }
    }
    return;
}

// Perform multiplication with the weight and the activation
DataPackage *MultiplierWS::perform_operation_2_operands(DataPackage *pck_left, DataPackage *pck_top, DataPackage *pck_local)
{
    // Extracting the values

    data_t psum = pck_top ? pck_top->get_data() : 0;
    data_t weight = pck_local ? pck_local->get_data() : 0;
    data_t result; // Result of the operation
    result = pck_left->get_data() * weight + psum;

#ifdef DEBUG_MSWITCH_FUNC
    std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierWS " << this->num << " has performed a MAC" << std::endl;
#endif

    // Creating the result package with the output
    DataPackage *result_pck = new DataPackage(sizeof(data_t), result, PSUM, this->num, this->VN, MULTIPLIER); // TODO the size of the package corresponds with the data size
    // Adding to the creation list to be deleted afterward
    // this->psums_created.push_back(result_pck);
    this->mswitchStats.n_multiplications++; // Track a multiplication
    return result_pck;
}

// Si es la primera iteracion no recibas del fw link
// Si es la ultima iteracion no envies al fw link
void MultiplierWS::cycle()
{
    this->mswitchStats.total_cycles++;
    this->receive(); // From top and left

    // std::cout <<"Cycle " << local_cycle <<  "(" << row_num << "," << col_num << ")"<<" prop="<<prop << " flip_counter="<<flip_counter<<"\n";

    if (flip_counter >= 0) {
        if (flip_counter == 0) {
            prop = !prop;
            flip_ready = false;
        }
        flip_counter--;
    }

    if(!act_limit_queue.empty()) {
        if(act_limit_queue.front().cycle == this->local_cycle) {
            auto limit = act_limit_queue.front();
            setActivationLimit(limit.T_M);
            // std::cout<<"[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Set activation limit to "<<activation_limit<<"\n";
            act_limit_queue.pop_front();

            if(row_num == ms_rows -1) {
                if(this->col_num <= limit.T_N -1) {
                    this->do_output = true;
                } else {
                    this->do_output = false;
                }
            }
        }
    }

    DataPackage* weight = nullptr;
    DataPackage* activation = nullptr;
    DataPackage* psum_in = nullptr;

    // Check top_weight_fifo for WEIGHT
    if (!top_weight_fifo->isEmpty()) {
        weight = top_weight_fifo->pop();
    }

    // Check top_psum_fifo for PSUM
    if (!top_psum_fifo->isEmpty()) {
        psum_in = top_psum_fifo->pop();
    }

    // Check left_fifo for Activation (b)
    if (!left_fifo->isEmpty()) {
        activation = left_fifo->pop();
    }

    data_t result = 0;
    DataPackage* psum_to_send = nullptr;
    DataPackage* weight_to_send = nullptr;
    DataPackage* activation_to_send = nullptr;

    if (weight) {
        weight_to_send = weight;
        weight_count++;
        // std::cout << "[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Received WEIGHT: " << weight->get_data() <<" prop="<<prop << std::endl;
    }

    if (activation) {
        activation_to_send = activation;
        // std::cout<< "[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Received ACTIVATION: " << activation->get_data()<<" prop="<<prop << std::endl;
    }

    data_t psum_in_data = 0;
    if(psum_in) {
        psum_in_data = psum_in->get_data();
        delete psum_in;
        // std::cout << "[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Received PSUM: " << psum_in->get_data()<<" prop="<<prop << std::endl;
    }

    if (weight_count >= weight_limit) {
        weight_count = 0;
        preload_finished = true;
        flip_ready = true;
        delete weight_to_send;
        weight_to_send = nullptr; // Do not forward more weights
    }

    // Double buffering logic
    if (prop) { // PROPAGATE
        // Sync fifo can get empty
        if (weight) {
            // std::cout<<"[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Loading WEIGHT1: " << weight->get_data()<<" prop="<<prop << std::endl;
            weight1 = weight->get_data();
        }

        if(activation) {
            result = activation->get_data() * weight2 + psum_in_data;
            c1_counter++;
            // std::cout<<"[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Performing MAC with ACTIVATION: " << activation->get_data()
            //          <<" and WEIGHT2: "<<weight2<<" PSUM_IN: "<<psum_in_data<<" RESULT: "<<result<<" prop="<<prop << std::endl;
            psum_to_send = new DataPackage(sizeof(data_t), result, COMPUTED_PSUM, this->num, this->VN, MULTIPLIER);

            flip_ready = false;
        }

        if(c1_counter >= activation_limit) {
            // std::cout<<"[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Flip ready after c1_counter reached activation limit."<<" prop="<<prop << std::endl;
            c1_counter = 0;
            flip_ready = true;
        }
    } else { // OTHERWISE
        if (weight) {
            // std::cout<<"[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Loading WEIGHT2: " << weight->get_data()<<" prop="<<prop << std::endl;
            weight2 = weight->get_data();
        }

        // some PE has not been flipped
        if(activation) {
            c2_counter++; 
            result = activation->get_data() * weight1 + psum_in_data;
            // std::cout<<"[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Performing MAC with ACTIVATION: " << activation->get_data()
            //          <<" and WEIGHT1: "<<weight1<<" PSUM_IN: "<<psum_in_data<<" RESULT: "<<result<<" prop="<<prop << std::endl;
            psum_to_send = new DataPackage(sizeof(data_t), result, COMPUTED_PSUM, this->num, this->VN, MULTIPLIER);
            
            flip_ready = false;
        }

        if(c2_counter >= activation_limit) {
            // std::cout<<"[MultiplierWS] Cycle " << local_cycle << " (" << row_num << "," << col_num << ") Flip ready after c2_counter reached activation limit."<<" prop="<<prop << std::endl;
            c2_counter = 0;
            flip_ready = true;
        }
    }

    if (psum_to_send) {
        if(row_num == ms_rows - 1) {
            if (do_output) {
                accbuffer_fifo->push(psum_to_send);
            } else {
                delete psum_to_send;
            }
        } else {
            bottom_psum_fifo->push(psum_to_send);
        }
    }

    if(weight_to_send) {
        if(row_num == ms_rows -1) {
            delete weight_to_send;
        } else {
            bottom_weight_fifo->push(weight_to_send);
        }
    }

    if(activation_to_send) {
        if(col_num == ms_cols -1) {
            delete activation_to_send;
        } else {
            right_fifo->push(activation_to_send);
        }
    }
    
    this->send();
    local_cycle += 1;
}

void MultiplierWS::printConfiguration(std::ofstream &out, unsigned int indent)
{
    out << ind(indent) << "{" << std::endl;
    out << ind(indent + IND_SIZE) << "\"VN\" : " << this->VN << std::endl;
    out << ind(indent) << "}"; // Take care. Do not print endl here. This is parent respo
}

void MultiplierWS::printStats(std::ofstream &out, unsigned int indent)
{

    out << ind(indent) << "{" << std::endl; // TODO put ID
    this->mswitchStats.print(out, indent + IND_SIZE);
    // Printing Fifos

    out << ind(indent + IND_SIZE) << ",\"TopWeightFifo\" : {" << std::endl;
    this->top_weight_fifo->printStats(out, indent + IND_SIZE + IND_SIZE);
    out << ind(indent + IND_SIZE) << "}," << std::endl; 

    out << ind(indent + IND_SIZE) << ",\"TopPsumFifo\" : {" << std::endl;
    this->top_psum_fifo->printStats(out, indent + IND_SIZE + IND_SIZE);
    out << ind(indent + IND_SIZE) << "}," << std::endl; 

    out << ind(indent + IND_SIZE) << "\"LeftFifo\" : {" << std::endl;
    this->left_fifo->printStats(out, indent + IND_SIZE + IND_SIZE);
    out << ind(indent + IND_SIZE) << "}," << std::endl; // Take care. Do not print endl here. This is parent responsability

    out << ind(indent + IND_SIZE) << "\"RightFifo\" : {" << std::endl;
    this->right_fifo->printStats(out, indent + IND_SIZE + IND_SIZE);
    out << ind(indent + IND_SIZE) << "}," << std::endl;
    ; // Take care. Do not print endl here. This is parent responsability

    out << ind(indent + IND_SIZE) << "\"BottomWeightFifo\" : {" << std::endl;
    this->bottom_weight_fifo->printStats(out, indent + IND_SIZE + IND_SIZE);
    out << ind(indent + IND_SIZE) << "}," << std::endl;

    out << ind(indent + IND_SIZE) << "\"BottomPsumFifo\" : {" << std::endl;
    this->bottom_psum_fifo->printStats(out, indent + IND_SIZE + IND_SIZE);
    out << ind(indent + IND_SIZE) << "}," << std::endl;
    ; // Take care. Do not print endl here. This is parent responsability

    out << ind(indent + IND_SIZE) << "\"OutputFifo\" : {" << std::endl;
    this->accbuffer_fifo->printStats(out, indent + IND_SIZE + IND_SIZE);
    out << ind(indent + IND_SIZE) << "}" << std::endl; // Take care. Do not print endl here. This is parent responsability

    out << ind(indent) << "}";
}

void MultiplierWS::printEnergy(std::ofstream &out, unsigned int indent)
{
    /*
         This component prints:
             - MSwitch multiplications
             - MSwitch forwardings
             - Counters for the FIFOs (registers in TPU, ie., we will use the access count of a register):
                 * top_fifo: the fifo that is used to receive the weights
                 * left_fifo: Fifo used to receive the activations
                 * right_fifo: Fifo used to send the activations
                 * bottom_fifo: FIFO used to send the weights
                 * output_fifo: FIFO used to store final result once accumulated
   */

    // Multiplier counters

    out << ind(indent) << "MULTIPLIER MULTIPLICATION=" << this->mswitchStats.n_multiplications;
    out << ind(indent) << " CONFIGURATION=" << this->mswitchStats.n_configurations << std::endl;

    // Fifo counters
    top_weight_fifo->printEnergy(out, indent);
    top_psum_fifo->printEnergy(out, indent);
    left_fifo->printEnergy(out, indent);
    right_fifo->printEnergy(out, indent);
    bottom_weight_fifo->printEnergy(out, indent);
    bottom_psum_fifo->printEnergy(out, indent);
    accbuffer_fifo->printEnergy(out, indent);
}
