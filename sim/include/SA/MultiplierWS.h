// Created 27/10/2020

#ifndef __MultiplierWS__h
#define __MultiplierWS__h

#include <vector>
#include <deque>
#include <utility>
#include "Config.h"
#include "Connection.h"
#include "DataPackage.h"
#include "Fifo.h"
#include "Stats.h"
#include "Unit.h"
#include "types.h"
/*
 */

class MultiplierWS : public Unit {
   private:
    SyncFifo* top_weight_fifo;   // Packages received from top (i.e., weights)
    SyncFifo* top_psum_fifo;     // Packages received from top (i.e., psums)
    SyncFifo* left_fifo;  // Packages recieved from legt (i.e., activations)
    SyncFifo* right_fifo;  // Packages to be sent to the right (i.e., activations)
    SyncFifo* bottom_weight_fifo;  // Packages to be sent to the bottom (i.e., weights)
    SyncFifo* bottom_psum_fifo;    // Packages to be sent to the bottom (i.e., psums)
    SyncFifo* accbuffer_fifo;  // Psum ready to be sent to the parent

    Connection* left_connection;    // To the left neighbour or memory port
    Connection* right_connection;   // To the right neighbour
    Connection* top_connection;     // To the top neighbour or the memory port
    Connection* bottom_connection;  // Input from the neighbour
    Connection* accbuffer_connection;  // To the accbuffer to keep OS
    cycles_t latency;                  // latency in number of cycles
    int row_num;
    int col_num;
    int num;  // General num, just used for information (num = row_num*ms_cols +
              // col_num)
    // This values are in esence the size of a single element in the
    // architecture (by default)
    unsigned int input_ports;
    unsigned int output_ports;
    unsigned int forwarding_ports;
    unsigned int buffers_capacity;
    unsigned int port_width;
    unsigned int ms_rows;
    unsigned int ms_cols;

    cycles_t local_cycle;
    MultiplierOSStats
        mswitchStats;  // Object to track the behaviour of the MSwitch

    // Signals
    unsigned int VN;
    bool forward_right = false;  // Based on rows (windows) left and dimensions
    bool forward_bottom = false;

    WSMeshControllerState current_state;  // Stage to control what to do according to the state
    data_t weight1 = 0;
    data_t weight2 = 0;

    unsigned int c1_counter = 0, c2_counter = 0;
    unsigned int weight_limit = 0;
    unsigned int weight_count = 0;
    unsigned int activation_limit = 0;
    unsigned int activation_count = 0;

    bool prop = false; // Propagate signal for double buffering

    bool preload_finished = false;

    bool flip_ready = true;
    bool do_flip = false;
    int flip_counter = -1;

    struct ActLimit {
        int cycle;
        unsigned int T_K;
        unsigned int T_M;
        unsigned int T_N;
    };

    std::deque<ActLimit> act_limit_queue;
    bool do_output = false;

   public:
    MultiplierWS(id_t id,
                 std::string name,
                 int row_num,
                 int col_num,
                 Config stonne_cfg);
    MultiplierWS(id_t id,
                 std::string name,
                 int row_num,
                 int col_num,
                 Config stonne_cfg,
                 Connection* left_connection,
                 Connection* right_connection,
                 Connection* top_connection,
                 Connection* bottom_connection);
    ~MultiplierWS();
    void setPropSignal(bool prop); // Set the propagate signal
    void setActivationLimit(unsigned int limit); // Set the activation limit for auto-flip
    void preload();
    void MatrixMultiply(tile_t T_M, tile_t T_K, tile_t T_N);
    void MatrixMultiplyFlip(tile_t T_M, tile_t T_K, tile_t T_N);
    void setTopConnection(Connection* top_connection);  // Set the top
                                                        // connection
    void setLeftConnection(
        Connection* left_connection);  // Set the left connection
    void setRightConnection(
        Connection* right_connection);  // Set the right connection
    void setBottomConnection(Connection* bottom_connection);
    void setAccBufferConnection(Connection* accbuffer_connection);

    void send();     // Send right, bottom and psum fifos
    void receive();  // Receive from top and left

    DataPackage* perform_operation_2_operands(
        DataPackage* pck_left,
        DataPackage* pck_top,
        DataPackage* pck_local);  // Perform multiplication and returns result.

    void cycle();  // Computing a cyclels
    void resetSignals();

    // Configure the forwarding signals that indicate if this ms has to forward
    // data to the bottom and or right neighbours
    void configureBottomSignal(bool bottom_signal);
    void configureRightSignal(bool right_signal);
    void setVirtualNeuron(unsigned int VN);

    void printConfiguration(
        std::ofstream& out,
        unsigned int indent);  // This function prints the configuration of
                               // MSwitch such us the VN ID
    void printStats(std::ofstream& out, unsigned int indent);
    void printEnergy(std::ofstream& out, unsigned int indent);

    bool is_weights_preloaded() { return preload_finished; }
    void reset_weights_preloaded() { preload_finished = false; }
    bool has_flipped() const { return flip_ready; }

    void reset_fifos() {
      top_weight_fifo->reset();
      top_psum_fifo->reset();
      left_fifo->reset();
      right_fifo->reset();
      bottom_weight_fifo->reset();
      bottom_psum_fifo->reset();
      accbuffer_fifo->reset();
    }
};

#endif
