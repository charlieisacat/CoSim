
#ifndef __MULTIPLIERNETWORK__H__
#define __MULTIPLIERNETWORK__H__

#include "Connection.h"
#include "MSwitch.h"
#include "DSwitch.h"
#include "Unit.h"
#include <iostream>
#include "CompilerMSN.h"
#include "Tile.h"
#include "DNNLayer.h"
#include "Region.h"
#include <assert.h>

#include <map>

class MultiplierNetwork : public Unit{
public:
    /*
       By the default the implementation of the MS just receives a single element, calculate a single psum and/or send a single input activation to the neighbour. This way, the parameters
       input_ports, output_ports and forwarding_ports will be set as the single data size. If this implementation change for future tests, this can be change easily bu mofifying these three parameters.
     */
    MultiplierNetwork(id_t id, std::string name) : Unit(id, name){}
    virtual ~MultiplierNetwork() {}
    //set connections from the distribution network to the multiplier network
    virtual void setInputConnections(std::map<int, Connection*> input_connections) {assert(false);}
    //Set connections from the Multiplier Network to the Reduction Network
    virtual void setOutputConnections(std::map<int, Connection*> output_connections) {assert(false);}
    virtual void cycle() {assert(false);}
    virtual void configureSignals(Tile* current_tile, DNNLayer* dnn_layer, unsigned int ms_size, unsigned int n_folding) {assert(false);}
    virtual void configureSignals(Tile* current_tile, Region* region, unsigned int ms_size, unsigned int n_folding) {assert(false);}
    virtual void configureSparseSignals(std::vector<SparseVN> sparseVNs, DNNLayer* dnn_layer, unsigned int ms_size) {assert(false);}

    virtual void resetSignals() {assert(false);}
    virtual void printConfiguration(std::ofstream& out, unsigned int indent) {assert(false);}
    virtual void printStats(std::ofstream &out, unsigned int indent) {assert(false);}
    virtual void printEnergy(std::ofstream& out, unsigned int indent) {assert(false);}

    virtual void preload(){assert(false);}
    virtual void MatrixMultiply(tile_t T_M, tile_t T_K, tile_t T_N){assert(false);} 
    virtual void MatrixMultiplyStay(tile_t T_M, tile_t T_K, tile_t T_N) { assert(false); }
    virtual void MatrixMultiplyFlip(tile_t T_M, tile_t T_K, tile_t T_N) { assert(false); }
    virtual void setActivationLimit(unsigned int limit) { assert(false); }

    virtual bool weights_preloaded(){assert(false); return false;}
    virtual void reset_weights_preloaded() { assert(false); }
    virtual void reset_fifos() { assert(false); }
    
    virtual bool psums_received(){assert(false); return false;}
    virtual void reset_psums_received() { assert(false); }
    virtual bool is_compute_finished() { assert(false); return false; }
    virtual void reset_compute_finished() { assert(false); }

    virtual bool has_array_flipped() { assert(false); return false; }
    virtual bool has_array_first_row_flipped() { assert(false); return false; }
};
#endif 
