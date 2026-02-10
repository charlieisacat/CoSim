#ifndef _TESTBENCH_H
#define _TESTBENCH_H
void sequential_layer(unsigned int R, unsigned int S, unsigned int C, unsigned int K, unsigned int G,  unsigned int N, unsigned int X, unsigned int Y, unsigned int strides,
data_t* input, data_t* filters, data_t * outputs);

void sequential_layer_v2(unsigned int M, unsigned int N, unsigned int K,
data_t* input, data_t* filters, data_t * outputs);

#endif
