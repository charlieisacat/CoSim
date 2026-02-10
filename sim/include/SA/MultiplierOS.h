//Created 27/10/2020

#ifndef __MultiplierOS__h
#define __MultiplierOS__h

#include "types.h"
#include "DataPackage.h"
#include "Connection.h"
#include "Fifo.h"
#include "Unit.h"
#include <vector>
#include "Config.h"
#include "Stats.h"
#include <deque>
#include <utility>
/*
*/

class MultiplierOS : public Unit {
private:
  SyncFifo* top_weight_fifo;   // Packages received from top (i.e., weights)
  SyncFifo* top_psum_fifo;     // Packages received from top during compute (psums)
  SyncFifo* top_preload_psum_fifo; // Packages received from top during preload (initial D matrix/psums)
  SyncFifo* left_fifo; //Packages recieved from legt (i.e., activations)
  SyncFifo* right_fifo; //Packages to be sent to the right (i.e., activations)
  SyncFifo* bottom_fifo; //Packages to be sent to the bottom (i.e., weights)
  SyncFifo* psum_fifo; // Packages received from top (i.e., psums) and to be sent to bottom/AB
  SyncFifo* accbuffer_fifo; //Psum ready to be sent to the parent

    Connection* left_connection;  // To the left neighbour or memory port
    Connection* right_connection; //To the right neighbour
    Connection* top_connection; //To the top neighbour or the memory port
    Connection* bottom_connection; //Input from the neighbour 
    Connection* accbuffer_connection; //To the accbuffer to keep OS
    cycles_t  latency;  //latency in number of cycles
    int row_num;
    int col_num;
    int num; //General num, just used for information (num = row_num*ms_cols + col_num)
    //This values are in esence the size of a single element in the architecture (by default)
    unsigned int input_ports;
    unsigned int output_ports;
    unsigned int forwarding_ports;
    unsigned int buffers_capacity;
    unsigned int port_width; 
    unsigned int ms_rows;
    unsigned int ms_cols;
   
   
    cycles_t local_cycle;
    MultiplierOSStats mswitchStats; //Object to track the behaviour of the MSwitch

    //Signals
    unsigned int VN;
    bool forward_right=false; //Based on rows (windows) left and dimensions
    bool forward_bottom=false;

    OSMeshControllerState current_state;
    bool psum_received=false;
    DataPackage* computed_psum = nullptr;
    DataPackage* psum_to_send = nullptr;

    bool compute_finish = false;

    unsigned int activation_limit;
    unsigned int c1_counter, c2_counter;
    data_t c1, c2;

    bool prop = false;
    bool flip_ready = true;

    unsigned int preload_counter = 0;
    
    int flip_counter = -1;
    int stay_cvt_counter = -1;
    int no_stay_cvt_counter = -1;

    bool stay = true;

   public:
    MultiplierOS(id_t id, std::string name, int row_num, int col_num, Config stonne_cfg);
    MultiplierOS(id_t id, std::string name, int row_num, int col_num, Config stonne_cfg, Connection* left_connection, Connection* right_connection, Connection* top_connection, Connection* bottom_connection);
    ~MultiplierOS();
    void setActivationLimit(unsigned int limit); // Set the activation limit
    void setTopConnection(Connection* top_connection); //Set the top connection
    void setLeftConnection(Connection* left_connection); //Set the left connection
    void setRightConnection(Connection* right_connection);  //Set the right connection
    void setBottomConnection(Connection* bottom_connection);
    void setAccBufferConnection(Connection* accbuffer_connection);

    void send(); //Send right, bottom and psum fifos
    void receive();  //Receive from top and left

    //Perform multiplication (and accumulate psums) and returns result.
    DataPackage* perform_operation_2_operands(DataPackage* pck_left, DataPackage* pck_right, DataPackage* pck_psum); 
    
    void cycle(); //Computing a cyclels
    void resetSignals(); 

    //Configure the forwarding signals that indicate if this ms has to forward data to the bottom and or right neighbours
    void configureBottomSignal(bool bottom_signal); 
    void configureRightSignal(bool right_signal); 
    void setVirtualNeuron(unsigned int VN);

    void printConfiguration(std::ofstream& out, unsigned int indent);  //This function prints the configuration of MSwitch such us the VN ID
    void printStats(std::ofstream& out, unsigned int indent);
    void printEnergy(std::ofstream& out, unsigned int indent);

    void preload() { 
        top_preload_psum_fifo->set_cnt(1);
        flip_ready = false;
    }

    bool send_next_cycle = false;

    void MatrixMultiply(tile_t T_M, tile_t T_K, tile_t T_N) {
      // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
      //           << ", MultiplierOS (" << row_num << ", " << col_num
      //           << ")  prop:" << prop << " starts MatrixMultiply operation "
      //           <<" no_stay_cvt_counter="<< no_stay_cvt_counter 
      //           <<" stay="<<stay
      //           << std::endl;
        // if (no_stay_cvt_counter == -1 && stay) {
        if (no_stay_cvt_counter == -1) {
          // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
          //       << ", MultiplierOS (" << row_num << ", " << col_num
          //       << ")  prop:"<<prop<<" no stay anymore\n ";
            no_stay_cvt_counter = row_num + col_num;
            // no_stay_cvt_counter = col_num;
        }

    }

    void MatrixMultiplyStay(tile_t T_M, tile_t T_K, tile_t T_N) {

      // std::cout<< "[MSWITCH_FUNC] Cycle " << this->local_cycle
      //           << ", MultiplierOS (" << row_num << ", " << col_num
      //           << ")  prop:"<<prop<<" starts MatrixMultiplyStay operation "
      //           <<" stay_cvt_counter="<< stay_cvt_counter 
      //           <<" stay="<<stay
      //           << std::endl;

      if(stay_cvt_counter == -1) {
        // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
        //         << ", MultiplierOS (" << row_num << ", " << col_num
        //         << ")  prop:"<<prop<<" set to stay\n ";
        stay_cvt_counter = row_num + col_num;
        // stay_cvt_counter = col_num;
      }

    }

    void MatrixMultiplyFlip(tile_t T_M, tile_t T_K, tile_t T_N) {
      // TODO: cannot work with stay
      flip_counter = row_num + col_num;
      // flip_counter = col_num;
    }

    bool is_psums_received() { return psum_received; }
    void reset_psums_received() { psum_received = false; }
    bool has_flipped() const { return flip_ready; }
    bool is_compute_finished() const { return compute_finish; }
    void reset_compute_finished() { compute_finish = false; }

    void reset_fifos() {
        psum_to_send = nullptr;
        flip_ready = true;

        top_weight_fifo->reset();
        top_psum_fifo->reset();
        top_preload_psum_fifo->reset();
        left_fifo->reset();
        right_fifo->reset();
        bottom_fifo->reset();
        psum_fifo->reset();
        accbuffer_fifo->reset();
    }
};

#endif

