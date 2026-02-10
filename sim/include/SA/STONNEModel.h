#ifndef STONNEMODEL_H_
#define STONNEMODEL_H_

#include <string>
//#include "RSNetwork.h"
#include "MSNetwork.h"
#include "DSNetworkTop.h"
#include "ASNetwork.h"
#include "SDMemory.h"
#include "Connection.h"
#include "LookupTable.h"
#include "CollectionBus.h"
#include "Config.h"
#include "CompilerART.h"
#include "CompilerMSN.h"
#include "ReduceNetwork.h"
#include "DistributionNetwork.h"
#include "FENetwork.h"
#include "MemoryController.h"
#include "TemporalRN.h"
#include "OSMeshSDMemory.h"
#include "WSMeshSDMemory.h"
#include "OSMeshMN.h"
#include "WSMeshMN.h"
#include "Command.h"
#include "WSAccumulationBuffer.h"
#include "SAController.h"

class SAController;

class Stonne {
private:
    //Hardware paramenters
    Config stonne_cfg;
    unsigned int ms_size; //Number of multipliers
    unsigned int n_adders; //Number of adders obtained from ms_size
    DistributionNetwork* dsnet; //Distribution Network
    MultiplierNetwork* msnet; //Multiplier Network
    ReduceNetwork* asnet; //ART Network
    LookupTable* lt; //Lookuptable
    MemoryController* mem; //MemoryController abstraction (e.g., SDMemory from MAERI)
    Bus* collectionBus; //CollectionBus
    Connection* outputASConnection; //The last connection of the AS and input to the lookuptable
    Connection* outputLTConnection; //Output of the lookup table connection and write port to the SDMemory
    Connection** addersBusConnections; //Array of output connections between the adders and the bus
    Connection** BusMemoryConnections; //Array of output Connections between the bus and the memory. (Write output ports)
    
    WSAccumulationBuffer* wsAB; //WS style accumulation buffer
    SAController* sac;

    DataflowType dataflow;

    //Software parameters
    DNNLayer* dnn_layer; 
    Region* region; 
    Tile* current_tile;
    bool layer_loaded; //Indicates if the function loadDNN
    bool tile_loaded; 
    bool region_loaded;

    //Connection and cycle functions
    void connectMemoryandDSN(); 
    void connectMSNandDSN(); //Function to connect the multiplieers of the MSN to the last level switches in the DSN.
    void connectMSNandASN();
    void connectMSNandAB();
    void connectASNandBus(); //Connect the adders to the Collection bus
    void connectBusandMemory(); //Connect the bus and the memory write ports.
    void printStats();
    void printEnergy();
    void printGlobalStats(std::ofstream& out, unsigned int indent);
   
    // DEBUG PARAMETERS
    unsigned long time_ds;
    unsigned long time_ms;
    unsigned long time_as;
    unsigned long time_lt;
    unsigned long time_mem;
    //DEBUG functions
    void testDSNetwork(unsigned int num_ms);
    void testTile(unsigned int num_ms);
    void testMemory(unsigned int num_ms);

    //Statistics
    unsigned int n_cycles;   

    std::list<Command*> command_queue;

    unsigned int T_M;
    unsigned int T_N;
    unsigned int T_K;
    unsigned int M;
    unsigned int N;
    unsigned int K;
   
    uint64_t local_cycle = 0;
   
public:
    Stonne (Config stonne_cfg);
    ~Stonne();

    void cycle();
    void run();
    void reset();
    void preload(unsigned int T_N, unsigned int T_K, unsigned int offset, Command *cmd);
    void MatrixMultiply(unsigned int T_M, unsigned int T_K, unsigned int T_N, unsigned int _offset_to_MK, unsigned int _offset_to_MN, unsigned int _offset_to_KN, Command *cmd);
    void commitComamnd(CmdSeqNum seq_num);

    void configMem(address_t MK_address, address_t KN_address,
                   address_t output_address, unsigned int M, unsigned int N,
                   unsigned int K, unsigned int T_M, unsigned int T_N,
                   unsigned int T_K);
    SAController* get_sac(){ return this->sac;}

    bool finished();
};

#endif
//TO DO add enumerate.
