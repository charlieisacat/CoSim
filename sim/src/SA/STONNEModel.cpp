//
// Created by Francisco Munoz-Martinez on 18/06/19.
//
#include "SA/STONNEModel.h"

#include <assert.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <chrono>
#include <vector>
#include "SA/Config.h"
#include "SA/Tile.h"
#include "SA/types.h"
#include "SA/utility.h"
#include "cpptoml.h"

Stonne::Stonne(Config stonne_cfg) {
    this->stonne_cfg = stonne_cfg;

    this->ms_size = stonne_cfg.m_MSNetworkCfg.ms_size;
    this->layer_loaded = false;
    this->region_loaded = false;
    this->tile_loaded = false;
    this->outputASConnection =
        new Connection(stonne_cfg.m_SDMemoryCfg.port_width);
    this->outputLTConnection =
        new Connection(stonne_cfg.m_LookUpTableCfg.port_width);
    this->sac = new SAController(stonne_cfg.dataflow);
    this->sac->set_stonne_model(this);
    this->dataflow = stonne_cfg.dataflow;

    if (this->dataflow == DATAFLOW_WS) {
        this->msnet = new WSMeshMN(2, "WSMesh", stonne_cfg);
        this->mem = new WSMeshSDMemory(0, "WSMeshSDMemory", stonne_cfg,
                                       this->outputLTConnection);
        this->wsAB =
            new WSAccumulationBuffer(6, "WSAccumulationBuffer", stonne_cfg);
    } else if (this->dataflow == DATAFLOW_OS) {
        this->msnet = new OSMeshMN(2, "OSMesh", stonne_cfg);
        this->mem = new OSMeshSDMemory(0, "OSMeshSDMemory", stonne_cfg,
                                       this->outputLTConnection);
        this->wsAB =
            new WSAccumulationBuffer(6, "WSAccumulationBuffer", stonne_cfg);
    } else {
        assert(0);
    }
    // switch(DistributionNetwork). It is possible to create instances of other
    // DistributionNetworks.h
    this->dsnet = new DSNetworkTop(1, "DSNetworkTop", stonne_cfg);

    // Creating the ReduceNetwork according to the parameter specified by the
    // user
    assert(stonne_cfg.m_ASNetworkCfg.reduce_network_type == TEMPORALRN);
    this->asnet =
        new TemporalRN(3, "TemporalRN", stonne_cfg, outputASConnection);

    // TODO: not used any more
    // this->asnet->NPSumsConfiguration(this->T_K);

    this->collectionBus = new Bus(4, "CollectionBus", stonne_cfg);
    this->lt = new LookupTable(5, "LookUpTable", stonne_cfg, outputASConnection,
                               outputLTConnection);

    // switch(MemoryController). It is possible to create instances of other
    // MemoryControllers

    // Adding to the memory controller the asnet and msnet to reconfigure them
    // if needed
    this->mem->setReduceNetwork(asnet);
    this->mem->setMultiplierNetwork(msnet);

    // Calculating n_adders
    this->n_adders = this->ms_size - 1;
    // rsnet
    this->connectMemoryandDSN();
    this->connectMSNandDSN();

    this->connectMSNandAB();
    this->sac->set_wsaccumulation_buffer(this->wsAB);

    sac->set_memory_controller(this->mem);
    sac->set_multiplier_network(this->msnet);

    // DEBUG PARAMETERS
    this->time_ds = 0;
    this->time_ms = 0;
    this->time_as = 0;
    this->time_lt = 0;
    this->time_mem = 0;

    // STATISTICS
    this->n_cycles = 0;
}

Stonne::~Stonne() {
    delete this->dsnet;
    delete this->msnet;
    delete this->asnet;
    delete this->outputASConnection;
    delete this->outputLTConnection;
    delete this->lt;
    delete this->mem;
    delete this->collectionBus;
    if (region_loaded) {
        delete this->region;
    }
    if (layer_loaded) {
        delete this->dnn_layer;
    }
    if (tile_loaded) {
        delete this->current_tile;
    }
}

// Connecting the DSNetworkTop input ports with the read ports of the memory.
// These connections have been created by the module DSNetworkTop, so we just
// have to connect them with the memory.
void Stonne::connectMemoryandDSN() {
    std::vector<Connection*> DSconnections = this->dsnet->getTopConnections();
    // Connecting with the memory
    this->mem->setReadConnections(DSconnections);
}

// Connecting the multipliers of the mSN to the last level switches of the DSN.
// In order to do this link correct, the number of connections in the last level
// of the DSN (output connections of the last level switches) must match the
// number of multipliers. The multipliers are then connected to those
// connections, setting a link between them.
void Stonne::connectMSNandDSN() {
    std::map<int, Connection*> DNConnections =
        this->dsnet->getLastLevelConnections();  // Map with the DS connections
    this->msnet->setInputConnections(DNConnections);
}
// Connect the multiplier switches with the Adder switches. Note the number of
// ASs connection connectionss and MSs must be the identical

void Stonne::connectMSNandASN() {
    std::map<int, Connection*> RNConnections =
        this->asnet->getLastLevelConnections();  // Map with the AS connections
    this->msnet->setOutputConnections(RNConnections);
}

void Stonne::connectMSNandAB() {
    std::map<int, Connection*> ABConnections =
        this->wsAB->getWriteConnections();  // Map with the AS connections
    this->msnet->setOutputConnections(ABConnections);
}

void Stonne::connectASNandBus() {
    std::vector<std::vector<Connection*>> connectionsBus =
        this->collectionBus
            ->getInputConnections();  // Getting the CollectionBus Connections
    this->asnet->setMemoryConnections(
        connectionsBus);  // Send the connections to the ReduceNetwork to be
                          // connected according to its algorithm
}

void Stonne::connectBusandMemory() {
    std::vector<Connection*> write_port_connections =
        this->collectionBus->getOutputConnections();
    this->mem->setWriteConnections(write_port_connections);
}

void Stonne::run() {
    // Execute the cycles
    this->cycle();
}

bool Stonne::finished() {
    return sac->is_execution_finished();
}

#if 1
void Stonne::cycle() {
    sac->cycle();
    this->mem->cycle();
    this->wsAB->cycle();
    this->msnet->cycle();
    this->dsnet->cycle();

    local_cycle++;
    if(local_cycle % 100000 == 0) {
        std::cout << "Stonne cycle: " << local_cycle << std::endl;
    }

}
#endif

#if 0
void Stonne::cycle() {
    bool execution_finished = false;

    while (!execution_finished) {
        sac->cycle();

        auto start = std::chrono::steady_clock::now();
        this->mem->cycle();
        auto end = std::chrono::steady_clock::now();
        this->time_mem +=
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();
        start = std::chrono::steady_clock::now();
        // this->lt->cycle();
        end = std::chrono::steady_clock::now();
        this->time_lt +=
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();

        this->wsAB->cycle();

        this->time_as +=
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();
        start = std::chrono::steady_clock::now();
        this->msnet->cycle();
        end = std::chrono::steady_clock::now();
        this->time_ms +=
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();
        start = std::chrono::steady_clock::now();
        this->dsnet->cycle();
        end = std::chrono::steady_clock::now();
        this->time_ds +=
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();

        execution_finished = sac->is_execution_finished();
        this->n_cycles++;
    }

    // if (this->stonne_cfg.print_stats_enabled) {  // If sats printing is enable
    //     this->printStats();
    //     this->printEnergy();
    // }
    // std::cout << "Number of cycles running: " << this->n_cycles << std::endl;
    // std::cout << "Time mem: " << time_mem / 1000000 << std::endl;
    // std::cout << "Time lt: " << time_lt / 1000000 << std::endl;
    // std::cout << "Time as: " << time_as / 1000000 << std::endl;
    // std::cout << "Time ms: " << time_ms / 1000000 << std::endl;
    // std::cout << "Time ds: " << time_ds / 1000000 << std::endl;
    // std::cout << "Time routing ds: " <<
    // this->dsnet->get_time_routing()/1000000000 << std::endl;
}
#endif

// General function to print all the STATS
void Stonne::printStats() {
    std::cout << "Printing stats" << std::endl;

    std::ofstream out;
    unsigned int num_ms = this->stonne_cfg.m_MSNetworkCfg.ms_size;
    unsigned int dn_bw = this->stonne_cfg.m_SDMemoryCfg.n_read_ports;
    unsigned int rn_bw = this->stonne_cfg.m_SDMemoryCfg.n_write_ports;
    const char* output_directory = std::getenv("OUTPUT_DIR");
    std::string output_directory_str = "";
    if (output_directory != NULL) {
        std::string env_output_dir(output_directory);
        output_directory_str += env_output_dir + "/";
    }

    out.open(output_directory_str + "output_stats_layer_" + "Region" +
             "_architecture_MSes_" + std::to_string(num_ms) + "_dnbw_" +
             std::to_string(dn_bw) + "_" + "rn_bw_" + std::to_string(rn_bw) +
             "timestamp_" + std::to_string((int)time(NULL)) +
             ".txt");  // TODO Modify name somehow
    unsigned int indent = IND_SIZE;
    out << "{" << std::endl;

    // Printing input parameters
    this->stonne_cfg.printConfiguration(out, indent);
    out << "," << std::endl;

    // Printing layer configuration parameters
    this->region->printConfiguration(out, indent);
    out << "," << std::endl;

    // Printing tile configuration parameters
    if (tile_loaded) {
        this->current_tile->printConfiguration(out, indent);
        out << "," << std::endl;
    }

    // Printing ASNetwork configuration parameters (i.e., ASwitches
    // configuration for these VNs, flags, etc)
    this->asnet->printConfiguration(out, indent);
    out << "," << std::endl;

    this->msnet->printConfiguration(out, indent);
    out << "," << std::endl;

    // Printing global statistics
    this->printGlobalStats(out, indent);
    out << "," << std::endl;

    // Printing all the components  // DSNetworkTop //DSNetworks //DSwitches
    this->dsnet->printStats(out, indent);
    out << "," << std::endl;
    this->msnet->printStats(out, indent);
    out << "," << std::endl;
    this->asnet->printStats(out, indent);
    out << "," << std::endl;
    this->mem->printStats(out, indent);
    out << "," << std::endl;
    this->collectionBus->printStats(out, indent);
    out << std::endl;

    out << "}" << std::endl;
    out.close();
}

void Stonne::printEnergy() {
    std::ofstream out;

    unsigned int num_ms = this->stonne_cfg.m_MSNetworkCfg.ms_size;
    unsigned int dn_bw = this->stonne_cfg.m_SDMemoryCfg.n_read_ports;
    unsigned int rn_bw = this->stonne_cfg.m_SDMemoryCfg.n_write_ports;

    const char* output_directory = std::getenv("OUTPUT_DIR");
    std::string output_directory_str = "";
    if (output_directory != NULL) {
        std::string env_output_dir(output_directory);
        output_directory_str += env_output_dir + "/";
    }

    out.open(output_directory_str + "output_stats_layer_" + "region" +
             "_architecture_MSes_" + std::to_string(num_ms) + "_dnbw_" +
             std::to_string(dn_bw) + "_" + "rn_bw_" + std::to_string(rn_bw) +
             "timestamp_" + std::to_string((int)time(NULL)) +
             ".counters");  // TODO Modify name somehow
    unsigned int indent = 0;
    out << "CYCLES=" << this->n_cycles
        << std::endl;  // This is to calculate the static energy
    out << "[DSNetwork]" << std::endl;
    this->dsnet->printEnergy(out,
                             indent);  // DSNetworkTop //DSNetworks //DSwitches
    out << "[MSNetwork]" << std::endl;
    this->msnet->printEnergy(out, indent);
    out << "[ReduceNetwork]" << std::endl;
    this->asnet->printEnergy(out, indent);
    out << "[GlobalBuffer]" << std::endl;
    this->mem->printEnergy(out, indent);
    out << "[CollectionBus]" << std::endl;
    this->collectionBus->printEnergy(out, indent);
    out << std::endl;

    out.close();
}

// Local function to the accelerator to print the globalStats
void Stonne::printGlobalStats(std::ofstream& out, unsigned int indent) {
    // unsigned int
    // n_mswitches_used=this->current_tile->get_VN_Size()*this->current_tile->get_Num_VNs();
    // float percentage_mswitches_used = (float)n_mswitches_used /
    // (float)this->stonne_cfg.m_MSNetworkCfg.ms_size;
    out << ind(indent) << "\"GlobalStats\" : {" << std::endl;  // TODO put ID
    // out << ind(indent+IND_SIZE) << "\"N_mswitches_used\" : " <<
    // n_mswitches_used << "," << std::endl; out << ind(indent+IND_SIZE) <<
    // "\"Percentage_mswitches_used\" : " << percentage_mswitches_used << "," <<
    // std::endl;
    out << ind(indent + IND_SIZE) << "\"N_cycles\" : " << this->n_cycles
        << std::endl;
    out << ind(indent) << "}";  // Take care. Do not print endl here. This is
                                // parent responsability
}

void Stonne::testMemory(unsigned int num_ms) {
    for (int i = 0; i < 20; i++) {
        this->mem->cycle();
        this->dsnet->cycle();
        this->msnet->cycle();
    }
}
void Stonne::testTile(unsigned int num_ms) {
    Tile* tile = new Tile(3, 1, 1, 2, 1, 1, 1, 1, false);
    // Tile* tile = new Tile(CONV, 2,2,1,2,1,2,2,1);
    // tile->generate_signals(num_ms);
    std::map<std::pair<int, int>, adderconfig_t>
        switches_configuration;  // tile->get_switches_configuration();
    for (auto it = switches_configuration.begin();
         it != switches_configuration.end(); ++it) {
        std::pair<int, int> current_node(it->first);
        adderconfig_t conf = it->second;
        std::cout << "Switch " << std::get<0>(current_node) << ":"
                  << std::get<1>(current_node) << " --> "
                  << get_string_adder_configuration(it->second) << std::endl;
    }
}

void Stonne::testDSNetwork(unsigned int num_ms) {
    // BRoadcast test
    /*
   DataPackage* data_to_send = new DataPackage(32, 1, IACTIVATION, 0,
   BROADCAST); std::vector<DataPackage*> vector_to_send;
   vector_to_send.push_back(data_to_send);
   this->inputConnection->send(vector_to_send);
   */

    // Unicast test
    /*
    DataPackage* data_to_send = new DataPackage(32, 500, IACTIVATION, 0,
    UNICAST, 6); std::vector<DataPackage*> vector_to_send;
    vector_to_send.push_back(data_to_send);
    this->inputConnection->send(vector_to_send);
    */

    // Multicast test

    bool* dests = new bool[num_ms];  // 16 MSs
    for (int i = 0; i < num_ms; i++) {
        dests[i] = false;
    }

    // Enabling Destinations
    for (int i = 0; i < 6; i++)
        dests[i] = true;

    DataPackage* data_to_send =
        new DataPackage(32, 1, IACTIVATION, 0, MULTICAST, dests, num_ms);
    std::vector<DataPackage*> vector_to_send;
    vector_to_send.push_back(data_to_send);
    // this->inputDSConnection->send(vector_to_send);

    // Configuring the adders
    // First test
    std::map<std::pair<int, int>, adderconfig_t>
        switches_configuration;  // Adders configuration
    std::map<std::pair<int, int>, fl_t> fwlinks_configuration;
    std::pair<int, int> switch0(0, 0);
    switches_configuration[switch0] = FW_2_2;

    std::pair<int, int> switch1(2, 1);
    switches_configuration[switch1] = ADD_1_1_PLUS_FW_1_1;
    fwlinks_configuration[switch1] = SEND;

    std::pair<int, int> switch2(2, 2);
    switches_configuration[switch2] = ADD_3_1;
    fwlinks_configuration[switch2] = RECEIVE;

    //    asnet->addersConfiguration(switches_configuration);
    //   asnet->forwardingConfiguration(fwlinks_configuration);

    this->dsnet->cycle();  // TODO REVERSE THE ORDER!!!
    this->msnet->cycle();
    for (int i = 0; i < 7; i++) {
        this->lt->cycle();
        this->asnet->cycle();  // 2 to 1
    }

    delete[] dests;
}

void Stonne::reset() {
    this->mem->reset();
    region_loaded = false;
    tile_loaded = false;
    region = nullptr;
    current_tile = nullptr;
}

void Stonne::preload(unsigned int T_N,
                     unsigned int T_K,
                     unsigned int offset,
                     Command* cmd) {
    this->mem->preload(T_N, T_K, offset, cmd);
    this->msnet->preload();
}

void Stonne::MatrixMultiply(unsigned int T_M,
                            unsigned int T_K,
                            unsigned int T_N,
                            unsigned int _offset_to_MK,
                            unsigned int _offset_to_MN,
                            unsigned int _offset_to_KN,
                            Command* cmd) {
    this->mem->MatrixMultiply(T_M, T_K, T_N, _offset_to_MK, _offset_to_MN,
                              _offset_to_KN, cmd);
    this->msnet->MatrixMultiply(T_M, T_K, T_N);

    if (this->wsAB) {
        this->wsAB->MatrixMultiply(T_M, T_K, T_N, _offset_to_MK, _offset_to_MN,
                                   _offset_to_KN, cmd);
    }
}

void Stonne::commitComamnd(CmdSeqNum seq_num) {
    auto order_it = command_queue.begin();
    auto order_end_it = command_queue.end();

    while (order_it != order_end_it && (*order_it)->get_seq_num() <= seq_num) {
        order_it = command_queue.erase(order_it);
    }
}

void Stonne::configMem(address_t MK_address, address_t KN_address,
                       address_t output_address, unsigned int M, unsigned int N,
                       unsigned int K, unsigned int T_M, unsigned int T_N,
                       unsigned int T_K) {
    this->mem->config(MK_address, KN_address, output_address, M, N, K, T_M, T_N, T_K);
  this->wsAB->config(MK_address, KN_address, output_address, M, N, K, T_M, T_N, T_K);

  if (this->dataflow == DATAFLOW_WS)
    this->msnet->setActivationLimit(T_M);
  else if (this->dataflow == DATAFLOW_OS)
    this->msnet->setActivationLimit(T_K);
}