//
// Created by Francisco Munoz on 27/10/20.
//
#include "SA/WSMeshMN.h"
#include <assert.h>
#include "SA/utility.h"

// By the default the three ports values will be set as one single data size
WSMeshMN::WSMeshMN(id_t id, std::string name, Config stonne_cfg) : MultiplierNetwork(id, name)
{
    // Extracting the input parameters
    this->ms_rows = stonne_cfg.m_MSNetworkCfg.ms_rows;
    this->ms_cols = stonne_cfg.m_MSNetworkCfg.ms_cols;

    this->forwarding_ports = stonne_cfg.m_MSwitchCfg.forwarding_ports;
    this->buffers_capacity = stonne_cfg.m_MSwitchCfg.buffers_capacity;
    this->port_width = stonne_cfg.m_MSwitchCfg.port_width;
    // End of extracting the input parameters

    for (int i = 0; i < this->ms_rows; i++)
    { // From root to leaves (without the MSs)
        for (int j = 0; j < this->ms_cols; j++)
        { // From left to right of the structure
            std::string ms_str = "MultiplierWS " + std::to_string(i) + ":" + std::to_string(j);
            unsigned int ms_id = i * this->ms_cols + j;
            MultiplierWS *ms = new MultiplierWS(ms_id, ms_str, i, j, stonne_cfg);
            std::pair<int, int> rowandcolumn(i, j);
            mswitchtable[rowandcolumn] = ms;
        }
    }
    setPhysicalConnection(); // Set Mesh links (top, left, right, bottom)
}

WSMeshMN::~WSMeshMN()
{
    // Deleting vertical connections
    for (std::map<std::pair<int, int>, Connection *>::iterator it = verticalconnectiontable.begin(); it != verticalconnectiontable.end(); ++it)
    {
        delete it->second;
    }

    // horizontal connections
    for (std::map<std::pair<int, int>, Connection *>::iterator it = horizontalconnectiontable.begin(); it != horizontalconnectiontable.end(); ++it)
    {
        delete it->second;
    }

    // Multipliers
    for (std::map<std::pair<int, int>, MultiplierWS *>::iterator it = mswitchtable.begin(); it != mswitchtable.end(); ++it)
    {
        delete it->second;
    }

    // Delete accbuff connections also
    for (auto it : accbufferconnectiontable)
    {
        delete it.second;
    }
}

// Connect a set of connections coming from the DistributionNetwork to the multipliers
// The MESH will need ms_cols connections to deliver the weights and ms_rows connections to deliver
// the inputs. This way, in the map input_connections received, the first ms_cols connections will be connected
// to the first row of the mesh. The next ms_rows connections will be connected to the first column.
// Note that an assert will raise if the number of connections is not equal to ms_cols+ms_rows
void WSMeshMN::setInputConnections(std::map<int, Connection *> input_connections)
{
    std::cout << "ms_cols+ms_rows=" << this->ms_cols + this->ms_rows << std::endl;
    assert((this->ms_cols + this->ms_rows) == input_connections.size());

    for (int i = 0; i < this->ms_cols; i++)
    { // for every column
        Connection *conn = input_connections[i];
        std::pair<int, int> index_ms(0, i); // 0 is the first row of mswitches in the mesh
        MultiplierWS *ms = this->mswitchtable[index_ms];
        ms->setTopConnection(conn);
    }

    // Connecting the input connections
    for (int i = 0; i < this->ms_rows; i++)
    {
        Connection *conn = input_connections[i + this->ms_cols];
        std::pair<int, int> index_ms(i, 0); // 0 is the firt column
        MultiplierWS *ms = this->mswitchtable[index_ms];
        ms->setLeftConnection(conn);
    }
}

// Connect a set of OutputConnections coming out to the accumulation buffers to perform the OS behaviour.
void WSMeshMN::setOutputConnections(std::map<int, Connection *> output_connections)
{

    // assert((this->ms_rows*this->ms_cols) == output_connections.size());
    assert(this->ms_cols == output_connections.size());

    // Iterating over each multiplier
    int i = this->ms_rows - 1;
    for (int j = 0; j < this->ms_cols; j++)
    {
        Connection *conn = output_connections[j];
        std::pair<int, int> index_ms(i, j);
        MultiplierWS *ms = this->mswitchtable[index_ms];
        ms->setAccBufferConnection(conn);
    }
}

void WSMeshMN::setActivationLimit(unsigned int limit) {
    for (int i = 0; i < ms_rows; i++) {
        for (int j = 0; j < ms_cols; j++) {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = this->mswitchtable[ms_index];
            ms->setActivationLimit(limit);
        }
    }
}

// Creating and Allocating the connections of the forwarding links
void WSMeshMN::setPhysicalConnection()
{
    for (int i = 0; i < this->ms_rows; i++)
    { // From root to leaves (without the MSs)
        for (int j = 0; j < this->ms_cols; j++)
        { // From left to right of the structure
            std::pair<int, int> rowandcolumn(i, j);
            MultiplierWS *ms = mswitchtable[rowandcolumn];
            // Inserting top  and bottom connections
            if (i > 0)
            {
                Connection *vertical_conn = new Connection(port_width);
                verticalconnectiontable[rowandcolumn] = vertical_conn;
                ms->setTopConnection(vertical_conn);
                std::pair<int, int> mstop_index(i - 1, j);
                MultiplierWS *mstop = mswitchtable[mstop_index];
                mstop->setBottomConnection(vertical_conn); // Connecting top to bottom (the current one)
            }

            if (j > 0)
            {
                Connection *horizontal_conn = new Connection(port_width);
                horizontalconnectiontable[rowandcolumn] = horizontal_conn;
                ms->setLeftConnection(horizontal_conn);
                std::pair<int, int> msleft_index(i, j - 1);
                MultiplierWS *msleft = mswitchtable[msleft_index];
                msleft->setRightConnection(horizontal_conn);
            }
        }
    }
}

// std::map<std::pair<int,int>, Connection*> MSNetwork::getConnections() {
//     return this->connectiontable;
// }

std::map<std::pair<int, int>, MultiplierWS *> WSMeshMN::getMSwitches()
{
    return this->mswitchtable;
}

void WSMeshMN::configureSignals(Tile *current_tile, Region *region, unsigned int ms_rows, unsigned int ms_cols)
{
    CompilerMultiplierMesh *compiler = new CompilerMultiplierMesh();
    compiler->configureSignals(current_tile, region, ms_rows, ms_cols);
    std::map<std::pair<int, int>, bool> forwarding_bottom_signals = compiler->get_forwarding_bottom_enabled();
    std::map<std::pair<int, int>, bool> forwarding_right_signals = compiler->get_forwarding_right_enabled();
    std::map<std::pair<int, int>, unsigned int> ms_vn_configuration = compiler->get_ms_vn_configuration();

    // Configuring signals
    for (int i = 0; i < ms_rows; i++)
    {
        for (int j = 0; j < ms_cols; j++)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = this->mswitchtable[ms_index];
            bool bottom_signal = forwarding_bottom_signals[ms_index];
            bool right_signal = forwarding_right_signals[ms_index];
            unsigned int VN = ms_vn_configuration[ms_index];
            ms->configureBottomSignal(bottom_signal);
            ms->configureRightSignal(right_signal);
            ms->setVirtualNeuron(VN);
        }
    }

    delete compiler;
}

void WSMeshMN::configureSparseSignals(std::vector<SparseVN> sparseVNs, DNNLayer *dnn_layer, unsigned int ms_size)
{
    assert(false); // Not supported in TPU
}

void WSMeshMN::resetSignals()
{
    for (int i = 0; i < ms_rows; i++)
    {
        for (int j = 0; j < ms_cols; j++)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = this->mswitchtable[ms_index];
            ms->resetSignals();
        }
    }
}

void WSMeshMN::cycle()
{
    // Reverse order to the forwarding. The current cycle receives the data of the forwarding links sent in the previous cycle.
    for (int i = (this->ms_rows - 1); i >= 0; i--)
    {
        for (int j = (this->ms_cols - 1); j >= 0; j--)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = mswitchtable[ms_index];
            ms->cycle();
        }
    }
}

void WSMeshMN::reset_weights_preloaded() {
    for (int i = (this->ms_rows - 1); i >= 0; i--)
    {
        for (int j = (this->ms_cols - 1); j >= 0; j--)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = mswitchtable[ms_index];
            ms->reset_weights_preloaded();
        }
    }
}

void WSMeshMN::reset_fifos() {
    for (int i = (this->ms_rows - 1); i >= 0; i--)
    {
        for (int j = (this->ms_cols - 1); j >= 0; j--)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = mswitchtable[ms_index];
            ms->reset_fifos();
        }
    }
}

bool WSMeshMN::has_array_flipped() {
    std::pair<int, int> ms_index(0, 0);
    MultiplierWS* ms = mswitchtable[ms_index];
    return ms->has_flipped();
}

bool WSMeshMN::has_array_first_row_flipped() {
    for (int j = 0; j < static_cast<int>(ms_cols); ++j) {
        std::pair<int, int> ms_index(0, j);
        MultiplierWS* ms = mswitchtable[ms_index];
        if (!ms->has_flipped()) {
            return false;
        }
    }
    return true;
}

bool WSMeshMN::weights_preloaded() {
    bool received = true;
    for (int i = (this->ms_rows - 1); i >= 0; i--)
    {
        for (int j = (this->ms_cols - 1); j >= 0; j--)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = mswitchtable[ms_index];
            received = received && ms->is_weights_preloaded();
        }
    }

    return received;
}

void WSMeshMN::printConfiguration(std::ofstream &out, unsigned int indent)
{

    out << ind(indent) << "\"MSNetworkConfiguration\" : {" << std::endl;
    out << ind(indent + IND_SIZE) << "\"MSwitchConfiguration\" : [" << std::endl;
    for (int i = 0; i < this->ms_rows; i++)
    { // We print the mesh using a 1-D array
        for (int j = 0; j < this->ms_cols; j++)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = mswitchtable[ms_index];
            ms->printConfiguration(out, indent + IND_SIZE + IND_SIZE);
            if ((i * this->ms_cols + j) == ((this->ms_rows * this->ms_cols) - 1))
            {                     // If I am in the last Mswitch, the comma to separate the MSwitches is not added
                out << std::endl; // This is added because the call to ms print do not show it (to be able to put the comma, if neccesary)
            }
            else
            {
                out << "," << std::endl; // Comma and line break are added to separate with the next MSwitch in the array
            }
        }
    }
    out << ind(indent + IND_SIZE) << "]" << std::endl;

    out << ind(indent) << "}";
}

void WSMeshMN::printStats(std::ofstream &out, unsigned int indent)
{

    out << ind(indent) << "\"MSNetworkStats\" : {" << std::endl;
    // out << ind(indent+IND_SIZE) << "\"ms_size\" : " << this->ms_size  << std::endl; DSNetwork global parameters
    out << ind(indent + IND_SIZE) << "\"MSwitchStats\" : [" << std::endl; // One entry per DSwitch
    for (int i = 0; i < this->ms_rows; i++)
    { // From root to leaves (without the MSs)
        for (int j = 0; j < this->ms_cols; j++)
        {
            std::pair<int, int> ms_index(i, j);
            MultiplierWS *ms = mswitchtable[ms_index];
            ms->printStats(out, indent + IND_SIZE + IND_SIZE);
            if ((i * this->ms_cols + j) == ((this->ms_rows * this->ms_cols) - 1))
            {                     // If I am in the last Mswitch, the comma to separate the MSwitches is not added
                out << std::endl; // This is added because the call to ms print do not show it (to be able to put the comma, if neccesary)
            }
            else
            {
                out << "," << std::endl; // Comma and line break are added to separate with the next MSwitch in the array
            }
        }
    }
    out << ind(indent + IND_SIZE) << "]" << std::endl;
    out << ind(indent) << "}";
}

void WSMeshMN::printEnergy(std::ofstream &out, unsigned int indent)
{
    /*

      This component prints:
          - the forwarding wires
          - the mswitches counters
    */

    // Printing the vertical wires

    for (std::map<std::pair<int, int>, Connection *>::iterator it = verticalconnectiontable.begin(); it != verticalconnectiontable.end(); ++it)
    {
        Connection *conn = verticalconnectiontable[it->first];
        conn->printEnergy(out, indent, "MN_WIRE");
    }

    // Printing horizontal wires
    for (std::map<std::pair<int, int>, Connection *>::iterator it = horizontalconnectiontable.begin(); it != horizontalconnectiontable.end(); ++it)
    {
        Connection *conn = horizontalconnectiontable[it->first];
        conn->printEnergy(out, indent, "MN_WIRE");
    }

    // Printing the mswitches counters

    for (std::map<std::pair<int, int>, MultiplierWS *>::iterator it = mswitchtable.begin(); it != mswitchtable.end(); ++it)
    {
        MultiplierWS *ms = mswitchtable[it->first];
        ms->printEnergy(out, indent);
    }
}

void WSMeshMN::preload() {
    for (std::map<std::pair<int, int>, MultiplierWS *>::iterator it = mswitchtable.begin(); it != mswitchtable.end(); ++it)
    {
        MultiplierWS *ms = mswitchtable[it->first];
        ms->preload();
    }
}

void WSMeshMN::MatrixMultiply(tile_t T_M, tile_t T_K, tile_t T_N) {
    for (std::map<std::pair<int, int>, MultiplierWS *>::iterator it = mswitchtable.begin(); it != mswitchtable.end(); ++it)
    {
        MultiplierWS *ms = mswitchtable[it->first];
        ms->MatrixMultiply(T_M, T_K, T_N);
    }
}

void WSMeshMN::MatrixMultiplyFlip(tile_t T_M, tile_t T_K, tile_t T_N) {
    for (std::map<std::pair<int, int>, MultiplierWS *>::iterator it = mswitchtable.begin(); it != mswitchtable.end(); ++it)
    {
        MultiplierWS *ms = mswitchtable[it->first];
        ms->MatrixMultiplyFlip(T_M, T_K, T_N);
    }
}
