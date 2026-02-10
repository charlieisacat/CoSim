//Created by Francisco Munoz-Martinez on 13/06/2019

#include "SA/MultiplierOS.h"
#include <assert.h>
#include "SA/utility.h"
/*
*/

MultiplierOS::MultiplierOS(id_t id, std::string name, int row_num, int col_num,  Config stonne_cfg): 
Unit(id, name){
    this->row_num = row_num;
    this->col_num = col_num;
    //Extracting parameters from configuration file 
    this->latency = stonne_cfg.m_MSwitchCfg.latency;
    this->input_ports = stonne_cfg.m_MSwitchCfg.input_ports;
    this->output_ports = stonne_cfg.m_MSwitchCfg.output_ports;
    this->forwarding_ports = stonne_cfg.m_MSwitchCfg.forwarding_ports;
    this->buffers_capacity = stonne_cfg.m_MSwitchCfg.buffers_capacity;
    this->port_width = stonne_cfg.m_MSwitchCfg.port_width;
    this->ms_rows = stonne_cfg.m_MSNetworkCfg.ms_rows;
    this->ms_cols = stonne_cfg.m_MSNetworkCfg.ms_cols;
    //End of extracting parameters from the configuration file
 
    //Parameters initialization
    unsigned int left_cnt = 1;
    unsigned int acc_cnt = 1;
    unsigned int top_cnt = 1;

    if (row_num == ms_rows - 1) {
        acc_cnt = ms_cols - col_num;
    }

    if (col_num == 0) {
        left_cnt = row_num + 1;
    }

    if (row_num == 0) {
        top_cnt = col_num + 1;
    }
    
    // std::cout << "MultiplierOS (" << row_num << ", " << col_num << ") sync fifo left cnt: " << left_cnt<<" acc cnt:"<<acc_cnt <<" top cnt="<<top_cnt << std::endl;

    this->top_weight_fifo = new SyncFifo(buffers_capacity, top_cnt);
    this->top_psum_fifo = new SyncFifo(buffers_capacity);
    this->top_preload_psum_fifo = new SyncFifo(buffers_capacity, top_cnt);
    this->bottom_fifo = new SyncFifo(buffers_capacity);
    this->right_fifo = new SyncFifo(buffers_capacity);
    this->left_fifo = new SyncFifo(buffers_capacity, left_cnt);
    this->psum_fifo = new SyncFifo(buffers_capacity, acc_cnt);
    this->accbuffer_fifo = new SyncFifo(buffers_capacity, acc_cnt);
    //End parameters initialization    

    this->left_connection = nullptr;
    this->right_connection = nullptr;
    this->top_connection = nullptr;
    this->bottom_connection = nullptr;
    this->accbuffer_connection = nullptr;

    this->local_cycle=0;
    this->forward_right=false; //Based on rows (windows) left and dimensions
    this->forward_bottom=false; //Based on columns (filters) left and dimensions
    this->VN = 0;
    this->num = this->row_num * this->ms_cols + this->col_num; //Multiplier ID, used just for information

    this->activation_limit = 1;

    psum_to_send = nullptr;
    psum_received = false;

    // std::cout<<"MultiplierOS "<<this->num<<" is created"<<std::endl;

    this->c1 = 0;
    this->c2 = 0;

    this->c1_counter = 0;
    this->c2_counter = 0;
}

MultiplierOS::MultiplierOS(id_t id, std::string name, int row_num, int col_num,  Config stonne_cfg, Connection* left_connection, Connection* right_connection, Connection* top_connection, Connection* bottom_connection) : MultiplierOS(id, name, row_num, col_num, stonne_cfg) {
    this->setLeftConnection(left_connection);
    this->setRightConnection(right_connection);
    this->setTopConnection(top_connection);
    this->setBottomConnection(bottom_connection);

}

MultiplierOS::~MultiplierOS() {
    delete this->left_fifo;
    delete this->right_fifo;
    delete this->top_weight_fifo;
    delete this->top_psum_fifo;
    delete this->top_preload_psum_fifo;
    delete this->bottom_fifo;
    delete this->psum_fifo;
    delete this->accbuffer_fifo;
}

void MultiplierOS::resetSignals() {

    this->forward_right=false; 
    this->forward_bottom=false;
    this->VN=0;
    this->c1 = 0;
    this->c2 = 0;
    this->c1_counter = 0;
    this->c2_counter = 0;
    flip_ready = true;
    while(!left_fifo->isEmpty()) {
        left_fifo->pop();
    }

    while(!right_fifo->isEmpty()) {
        right_fifo->pop();
    }

    while(!top_weight_fifo->isEmpty()) {
        top_weight_fifo->pop();
    }

    while(!top_psum_fifo->isEmpty()) {
        top_psum_fifo->pop();
    }

    while(!top_preload_psum_fifo->isEmpty()) {
        top_preload_psum_fifo->pop();
    }

    while(!bottom_fifo->isEmpty()) {
        bottom_fifo->pop();
    }

    while(!psum_fifo->isEmpty()) {
        psum_fifo->pop();
    }


}
void MultiplierOS::setLeftConnection(Connection* left_connection) { 
    this->left_connection = left_connection;
}
void MultiplierOS::setRightConnection(Connection* right_connection) { //Set the right connection of the ms
    this->right_connection = right_connection;
}

void MultiplierOS::setTopConnection(Connection* top_connection) {
    this->top_connection = top_connection;
}

void MultiplierOS::setBottomConnection(Connection* bottom_connection) { //Set the input connection of the switch
    this->bottom_connection = bottom_connection;
}

void MultiplierOS::setAccBufferConnection(Connection* accbuffer_connection) { //Set the output connection 
    this->accbuffer_connection = accbuffer_connection;
}

void MultiplierOS::setActivationLimit(unsigned int limit) {
    this->activation_limit = limit;
}

void MultiplierOS::configureBottomSignal(bool bottom_signal) {
    this->forward_bottom = bottom_signal;
}

void MultiplierOS::configureRightSignal(bool right_signal) {
    this->forward_right = right_signal;
}

void MultiplierOS::setVirtualNeuron(unsigned int VN) {
    this->VN = VN;
    this->mswitchStats.n_configurations++;
#ifdef DEBUG_MSWITCH_CONFIG
    std::cout << "[MSWITCH_CONFIG] MultiplierOS "  << ". VirtualNeuron: " << this->VN << std::endl;
#endif

}

void MultiplierOS::send() {  // Send the result through the outputConnection
  // Sending weights and psums to bottom
  std::vector<DataPackage*> vector_to_send_bottom;
  // Weights
  if (!bottom_fifo->isEmpty()) {
    DataPackage* data_to_send = bottom_fifo->pop();
    if (data_to_send) {
        vector_to_send_bottom.push_back(data_to_send);
    }
  }

  // TODO: there is a timing problem that multiple psums are sent
  // psum should be sent and received by next PE in the next cycle 
  // Psums (if going to bottom)
  if (this->row_num != this->ms_rows - 1) {
    // while (!psum_fifo->isEmpty()) {
    if (!psum_fifo->isEmpty()) {
      DataPackage* data_to_send = psum_fifo->pop();
      if (data_to_send) {
        vector_to_send_bottom.push_back(data_to_send);
      }
    }
  }
  if (vector_to_send_bottom.size() > 0) {
    this->mswitchStats.n_bottom_forwardings_send++;
    this->bottom_connection->send(vector_to_send_bottom);
  }

  // Sending activations
  std::vector<DataPackage*> vector_to_send_activations;
  if (!right_fifo->isEmpty()) {
    DataPackage* data_to_send = right_fifo->pop();
    if (data_to_send) {
        vector_to_send_activations.push_back(data_to_send);
    }
  }
  if (vector_to_send_activations.size() > 0) {
    this->mswitchStats.n_right_forwardings_send++;
    this->right_connection->send(vector_to_send_activations);
  }

  // Sending psums to Accumulation Buffer (if last row)
  if (this->row_num == this->ms_rows - 1) {
      std::vector<DataPackage*> vector_to_send_psums;
    
      if(!psum_fifo->isEmpty()) {
          DataPackage *data_to_send = psum_fifo->pop();
          if (data_to_send) {
            // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierOS (" << row_num << ", " << col_num << ") has forward a psum to the accumulation buffer" <<" addr="<<this->accbuffer_connection <<" data="<<data_to_send->get_data() << std::endl;
            vector_to_send_psums.push_back(data_to_send);
          }
      }

    //   if(!accbuffer_fifo->isEmpty()) {
    //     DataPackage *data_to_send = accbuffer_fifo->pop();
    //     if (data_to_send) {
    //         std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle << ", MultiplierOS (" << row_num << ", " << col_num << ") has forward a data to the accumulation buffer" <<" addr="<<this->accbuffer_connection <<" data="<<data_to_send->get_data() << std::endl;
    //         vector_to_send_psums.push_back(data_to_send);
    //     }
    //   }
      if(vector_to_send_psums.size() > 0) {
          this->accbuffer_connection->send(vector_to_send_psums);
      }
  }
}

void MultiplierOS::receive() {  //Receive a package either from the left or the top connection

  if (left_connection->existPendingData()) {
    std::vector<DataPackage*> data_received = left_connection->receive();
    assert(data_received.size() == 1);
    for (int i = 0; i < data_received.size(); i++) {
      DataPackage* pck = data_received[i];
      assert(pck->get_data_type() == IACTIVATION);
      // std::cout << "[MSWITCH] MultiplierOS (" << row_num << ", " << col_num
      //           << ") cycle " << local_cycle << " has received a ACT"
      //           << " from the left connection: " << pck->get_data() << "\n";

      this->mswitchStats.n_left_forwardings_receive++;

      left_fifo->push(pck);
    }
  }

    if (top_connection->existPendingData()) {
        std::vector<DataPackage*> data_received = top_connection->receive();
        // assert(data_received.size() == 1);
        for (int i = 0; i < data_received.size(); i++) {
            DataPackage* pck = data_received[i];
            assert(pck->get_data_type() == WEIGHT || pck->get_data_type() == PSUM || pck->get_data_type() == COMPUTED_PSUM);
            this->mswitchStats.n_top_forwardings_receive++;

            if (pck->get_data_type() == PSUM) {
              top_preload_psum_fifo->push(pck);
            } else if (pck->get_data_type() == COMPUTED_PSUM) {
                // std::cout<<"[MSWITCH] MultiplierOS (" << row_num << ", " << col_num
                //          << ") cycle " << local_cycle << " has received a COMPUTED_PSUM"
                //          << " from the top connection: " << pck->get_data() << "\n";
              top_psum_fifo->push(pck);

            } else {
              top_weight_fifo->push(pck);
              // std::cout<<"[MSWITCH] MultiplierOS (" << row_num << ", " << col_num
              //          << ") cycle " << local_cycle << " has received a WEIGHT"
              //          << " from the top connection: " << pck->get_data() << "\n";
            }
        }
    }
    return;
}

//Perform multiplication with the weight and the activation
DataPackage* MultiplierOS::perform_operation_2_operands(DataPackage* pck_left,
                                                        DataPackage* pck_right,
                                                        DataPackage* pck_psum) {
    // Extracting the values
    data_t result;  // Result of the operation
    data_t psum_data = 0;

    if (pck_psum) {
        psum_data = pck_psum->get_data();
    }

    // std::cout << "(" << row_num << ", " << col_num << ")"
    //           << "left value: " << pck_left->get_data()
    //           << " right value: " << pck_right->get_data()
    //           << " psum=" << psum_data << std::endl;
    result = pck_left->get_data() * pck_right->get_data() + psum_data;

    //Creating the result package with the output
    // TODO the size of the package corresponds with the data size
    DataPackage* result_pck = new DataPackage(sizeof(data_t), result, PSUM,
                                              this->num, this->VN, MULTIPLIER);
    // Adding to the creation list to be deleted afterward
    // this->psums_created.push_back(result_pck);
    this->mswitchStats.n_multiplications++;  // Track a multiplication
    return result_pck;
}

void MultiplierOS::cycle() {  // Computing a cycle
  //    std::cout << "MS: " << this->num << ". weight fifo size: " <<
  //    weight_fifo->size() << " N_Folding: " << n_folding << std::endl;
  this->mswitchStats.total_cycles++;
  this->receive();  // From top and left

  // computed_psum: psum from previous PE
  DataPackage* weight = nullptr;
  DataPackage* preload_psum = nullptr;
  DataPackage* activation = nullptr;

  psum_to_send = nullptr;

  if(flip_counter >= 0) {
    if(flip_counter == 0) {
      prop = !prop;
      flip_ready = false;
    }
    flip_counter--;
  }

  if(no_stay_cvt_counter >=0) {
    if(no_stay_cvt_counter ==0) {
      // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
      //           << ", MultiplierOS (" << row_num << ", " << col_num
      //           << ")  prop:"<<prop<<" no stay anymore\n ";
      stay = false;
    }
    no_stay_cvt_counter--;
  }

  if(stay_cvt_counter >=0) {
    if(stay_cvt_counter ==0) {
      stay = true;
    }
    stay_cvt_counter--;
  }

  if (!top_preload_psum_fifo->isEmpty()) {
    preload_psum = top_preload_psum_fifo->pop();
  }

  if (!top_weight_fifo->isEmpty()) {
    weight = top_weight_fifo->pop();
  }

  if (!left_fifo->isEmpty()) {
    activation = left_fifo->pop();
  }

  data_t preload_data = 0;
  if (preload_psum) {
    preload_data = preload_psum->get_data();
    preload_counter++;
  }

  if(preload_counter == ms_rows - row_num) {
    // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
    //           << ", MultiplierOS (" << row_num << ", " << col_num
    //           << ")  prop:"<<prop<<" has received a preload psum from the top connection: "
    //           << preload_data << std::endl;
    psum_received = true;
    preload_counter = 0;

    delete preload_psum;
    preload_psum = nullptr;
  }

  data_t result = 0;
  bool should_count = false;
  if (activation && weight) {
    // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
    //           << ", MultiplierOS (" << row_num << ", " << col_num
    //           << ")  prop:"<<prop<<" has received "
    //           << "activation=" << activation->get_data()
    //           << " weight=" << weight->get_data() << std::endl;
    // Perform multiplication
    result = activation->get_data() * weight->get_data();
    this->mswitchStats.n_multiplications++;
    should_count = true;

    flip_ready = false;
    compute_finish = false;
  }

  // if(activation) std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
  //           << ", MultiplierOS (" << row_num << ", " << col_num
  //           << ")  prop:"<<prop<<" has received an activation from the left connection: "
  //           << activation->get_data() << std::endl;

  //   if(weight) std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
  //           << ", MultiplierOS (" << row_num << ", " << col_num
  //           << ")  prop:"<<prop<<" has received a weight from the top connection: "
  //           << weight->get_data() << std::endl;

  if (prop) {
    // if (should_count)
    //   std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
    //             << ", MultiplierOS (" << row_num << ", " << col_num
    //             << ") prop:" << prop << " c1=" << c1 << " result=" << result
    //             << " c1 counter=" << c1_counter << " lmit="<<activation_limit << std::endl;
    c1 += result;

    if (should_count) c1_counter++;

    if (c1_counter == activation_limit) {
      // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
      //           << ", MultiplierOS (" << row_num << ", " << col_num
      //           << ")  prop:"<<prop<<" c1_counter =" << c1_counter <<" stay ="<<stay
      //           << " activation_limit=" << activation_limit << " ====  flip prop\n ";
      if (!stay) {
          prop = !prop;
      } else {
        c1_counter = 0;
      }

      flip_ready = true;
      compute_finish = true;
    }

    if (c2_counter == activation_limit) {
        // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
        //           << ", MultiplierOS (" << row_num << ", " << col_num
        //           << ") prop:" << prop << " c2 send psum: " << c2 << std::endl;
        psum_to_send = new DataPackage(sizeof(data_t), c2, COMPUTED_PSUM,
                                       this->num, this->VN, MULTIPLIER);
        c2_counter = 0;
        c2 = 0;
    } else {
        psum_to_send = computed_psum;
        computed_psum = nullptr;
    }

    if(psum_received) {
        // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
        //           << ", MultiplierOS (" << row_num << ", " << col_num
        //           << ")  prop:"<<prop<<" c2 has received a preload psum from the top connection: "
        //           << preload_data << std::endl;
        c2 = preload_data;
      flip_ready = true;
    }
  } else {
    // if (should_count)
    //   std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
    //             << ", MultiplierOS (" << row_num << ", " << col_num
    //             << ") prop:" << prop << " c2=" << c2 << " result=" << result
    //             << " c2 counter=" << c2_counter << std::endl;

    c2 += result;
    if (should_count) c2_counter++;

    if (c2_counter == activation_limit) {
      // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
      //           << ", MultiplierOS (" << row_num << ", " << col_num
      //           << ")  prop:"<<prop<<" c2_counter =" << c2_counter <<" stay ="<<stay 
      //           << " activation_limit=" << activation_limit << " ====  flip prop\n ";

      if (!stay) {
          prop = !prop;
      } else {
        c2_counter = 0;
      }

      flip_ready = true;
      compute_finish = true;
      // top_weight_fifo->reset();
      // left_fifo->reset();
    }

    if (c1_counter == activation_limit) {
        // std::cout << "[MSWITCH_FUNC] Cycle " << this->local_cycle
        //           << ", MultiplierOS (" << row_num << ", " << col_num
        //           << ") prop:" << prop << " c1 send psum: " << c1  << std::endl;
        psum_to_send = new DataPackage(sizeof(data_t), c1, COMPUTED_PSUM,
                                       this->num, this->VN, MULTIPLIER);
        c1_counter = 0;
        c1 = 0;
    } else {
        psum_to_send = computed_psum;
        computed_psum = nullptr;
    }

    if(psum_received) {
        // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
        //           << ", MultiplierOS (" << row_num << ", " << col_num
        //           << ")  prop:"<<prop<<" c1 has received a preload psum from the top connection: "
        //           << preload_data << std::endl;
        c1 = preload_data;
      flip_ready = true;
    }
  }

  if (activation) {
    // If we are in the last column we do
    if (col_num == ms_cols - 1) {
      // not forward the activation
      delete activation;
    } else {
      // Forwarding the activation to the right
      right_fifo->push(activation);
    }
  }

  if (weight) {
    if (row_num == ms_rows - 1) {
      delete weight;
    } else {
      bottom_fifo->push(weight);
    }
  }

  if (preload_psum) {
    bottom_fifo->push(preload_psum);
  }

  if (psum_to_send) {
    // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
    //           << ", MultiplierOS (" << row_num << ", " << col_num
    //           << ")  prop:"<<prop<<" has push to psum_fifo: "
    //           << psum_to_send->get_data() << std::endl;
    psum_fifo->push(psum_to_send);
    psum_to_send = nullptr;
  }

  // send first and receive later to avoid conflict
  if (!top_psum_fifo->isEmpty()) {
    assert(computed_psum == nullptr);
    computed_psum = top_psum_fifo->pop();
    // std::cout<<"[MSWITCH_FUNC] Cycle " << this->local_cycle
    //           << ", MultiplierOS (" << row_num << ", " << col_num
    //           << ")  prop:"<<prop<<" has pop from top psum fifo "<< computed_psum->get_data() << std::endl;
  }

  this->send();
  local_cycle += 1;
}

void MultiplierOS::printConfiguration(std::ofstream& out, unsigned int indent) {
    out << ind(indent) << "{" << std::endl; 
    out << ind(indent+IND_SIZE) << "\"VN\" : " << this->VN  << std::endl;
    out << ind(indent) << "}"; //Take care. Do not print endl here. This is parent respo    
}

void MultiplierOS::printStats(std::ofstream& out, unsigned int indent) {
    
    out << ind(indent) << "{" << std::endl; //TODO put ID
    this->mswitchStats.print(out, indent+IND_SIZE);
    //Printing Fifos 

    out << ind(indent+IND_SIZE) << "\"TopWeightFifo\" : {" << std::endl;
        this->top_weight_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);         
    out << ind(indent+IND_SIZE) << "}," << std::endl; //Take care. Do not print endl here. This is parent responsability

    out << ind(indent+IND_SIZE) << "\"TopPsumFifo\" : {" << std::endl;
        this->top_psum_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);
    out << ind(indent+IND_SIZE) << "}," << std::endl;

    out << ind(indent+IND_SIZE) << "\"TopPreloadPsumFifo\" : {" << std::endl;
        this->top_preload_psum_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);
    out << ind(indent+IND_SIZE) << "}," << std::endl;

    out << ind(indent+IND_SIZE) << "\"LeftFifo\" : {" << std::endl;
        this->left_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);
    out << ind(indent+IND_SIZE) << "}," << std::endl; //Take care. Do not print endl here. This is parent responsability

    out << ind(indent+IND_SIZE) << "\"RightFifo\" : {" << std::endl;
        this->right_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);
    out << ind(indent+IND_SIZE) << "}," << std::endl;; //Take care. Do not print endl here. This is parent responsability
   
    out << ind(indent+IND_SIZE) << "\"BottomFifo\" : {" << std::endl;
        this->bottom_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);
    out << ind(indent+IND_SIZE) << "}," << std::endl;; //Take care. Do not print endl here. This is parent responsability

    out << ind(indent+IND_SIZE) << "\"PsumFifo\" : {" << std::endl;
        this->psum_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);
    out << ind(indent+IND_SIZE) << "}," << std::endl;; //Take care. Do not print endl here. This is parent responsability

    out << ind(indent+IND_SIZE) << "\"OutputFifo\" : {" << std::endl;
        this->accbuffer_fifo->printStats(out, indent+IND_SIZE+IND_SIZE);
    out << ind(indent+IND_SIZE) << "}" << std::endl; //Take care. Do not print endl here. This is parent responsability

    out << ind(indent) << "}"; 
}

void MultiplierOS::printEnergy(std::ofstream& out, unsigned int indent) {
    /*
         This component prints:
             - MSwitch multiplications
             - MSwitch forwardings
             - Counters for the FIFOs (registers in TPU, ie., we will use the access count of a register):
                 * top_weight_fifo: fifo used to receive weights 
                 * top_psum_fifo: fifo used to receive psums during compute
                 * top_preload_psum_fifo: fifo used to receive psums during preload
                 * left_fifo: Fifo used to receive the activations
                 * right_fifo: Fifo used to send the activations
                 * bottom_fifo: FIFO used to send the weights
                 * output_fifo: FIFO used to store final result once accumulated
   */

    //Multiplier counters
    
    out << ind(indent) << "MULTIPLIER MULTIPLICATION=" << this->mswitchStats.n_multiplications;
    out << ind(indent) << " CONFIGURATION=" << this->mswitchStats.n_configurations << std::endl;

    //Fifo counters
    top_weight_fifo->printEnergy(out, indent);
    top_psum_fifo->printEnergy(out, indent);
    top_preload_psum_fifo->printEnergy(out, indent);
    left_fifo->printEnergy(out, indent);
    right_fifo->printEnergy(out, indent);
    bottom_fifo->printEnergy(out, indent);
    psum_fifo->printEnergy(out, indent);
    accbuffer_fifo->printEnergy(out, indent);
    
    
}


