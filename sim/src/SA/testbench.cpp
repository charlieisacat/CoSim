#include <iostream>
#include "SA/DNNLayer.h"
#include "SA/STONNEModel.h"
#include <assert.h>
#include "SA/types.h"


void sequential_layer(unsigned int R, unsigned int S, unsigned int C, unsigned int K, unsigned int G,  unsigned int N, unsigned int X, unsigned int Y, unsigned int strides, 
data_t* input, data_t* filters, data_t * outputs) {

    unsigned int OX=(X - R + strides) / strides;
    unsigned int OY=(Y - S + strides) / strides;
    K=K/G;
    C=C/G;
    unsigned int output_size_n = G*K*OX*OY;
    unsigned int input_size_n = G*C*X*Y;
    unsigned int filter_size=R*S*C;
    unsigned int size_oy=OY*K*G;
    unsigned int size_y=Y*G*C;
    for(int n=0; n<N; n++) {
        for(int g=0; g<G; g++) {
            for(int k=0; k<K; k++) {
                for(int ox=0; ox<OX; ox++) {
                    for(int oy=0; oy<OY; oy++) {
                        outputs[n*output_size_n + ox*size_oy + oy*K*G + g*K + k]=0.0;
                        for(int c=0; c<C;c++) {
                            for(int r=0;r<R;r++) {
                                for(int s=0;s<S;s++) {
                                    outputs[n*output_size_n + ox*size_oy + oy*K*G + g*K + k] += input[n*input_size_n+ ox*strides*size_y + oy*strides*C*G + r*size_y + s*C*G + g*C + c]*filters[g*K*filter_size + k*filter_size + r*S*C + s*C + c];
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}

void sequential_layer_v2(unsigned int M, unsigned int N, unsigned int K,
data_t* input, data_t* filters, data_t * outputs){
    for(int m=0; m<M; m++) {
        for(int n=0; n<N; n++) {
            outputs[m*N + n]=0.0;
            for(int k=0; k<K; k++) {
                outputs[m*N + n] += input[m*K + k]*filters[n*K + k];
            }
        }
    }
}



