// Runtime wrapper for calling Gemmini matmul from MLIR-generated code.
//
// This file is meant to be compiled and linked together with the object
// generated from MLIR (after lowering to LLVM). The MLIR side should declare
// and call:
//   func.func private @tiled_matmul_nn_auto_mlir(memref<?x?xi8>, memref<?x?xi8>, memref<?x?xi8>)
//     attributes { llvm.emit_c_interface }
//
// The `llvm.emit_c_interface` ABI passes pointers to StridedMemRefType.

#include <cstdio>
#include <cstdlib>
#include <vector>

// Gemmini headers (elem_t, tiled_matmul_auto, enums, etc.).
#include "include/gemmini.h"

// MLIR C runner ABI for memrefs.
#include "mlir/ExecutionEngine/CRunnerUtils.h"

extern "C" void tiled_matmul_nn_auto_mlir(StridedMemRefType<elem_t, 2> *A,
                                          StridedMemRefType<elem_t, 2> *B,
                                          StridedMemRefType<elem_t, 2> *C) {
  if (!A || !B || !C) {
    std::fprintf(stderr, "tiled_matmul_nn_auto_mlir: null memref\n");
    std::abort();
  }

  const size_t dim_I = (size_t)A->sizes[0];
  const size_t dim_K = (size_t)A->sizes[1];
  const size_t dim_K_B = (size_t)B->sizes[0];
  const size_t dim_J = (size_t)B->sizes[1];

  if (dim_K_B != dim_K) {
    std::fprintf(stderr,
                 "tiled_matmul_nn_auto_mlir: shape mismatch: A[%zu,%zu], B[%zu,%zu]\n",
                 dim_I, dim_K, dim_K_B, dim_J);
    std::abort();
  }

  if ((size_t)C->sizes[0] != dim_I || (size_t)C->sizes[1] != dim_J) {
    std::fprintf(stderr,
                 "tiled_matmul_nn_auto_mlir: shape mismatch: C[%zu,%zu], expected [%zu,%zu]\n",
                 (size_t)C->sizes[0], (size_t)C->sizes[1], dim_I, dim_J);
    std::abort();
  }

  // Require contiguous (or at least row-major unit-stride in minor dim).
  if (A->strides[1] != 1 || B->strides[1] != 1 || C->strides[1] != 1) {
    std::fprintf(stderr,
                 "tiled_matmul_nn_auto_mlir: only unit-stride minor dimension supported (got A.s1=%ld, B.s1=%ld, C.s1=%ld)\n",
                 (long)A->strides[1], (long)B->strides[1], (long)C->strides[1]);
    std::abort();
  }

  const size_t A_stride = (size_t)A->strides[0];
  const size_t B_stride = (size_t)B->strides[0];
  const size_t C_stride = (size_t)C->strides[0];

  // Defaults: no bias (D=null), no activation, identity scale.
  // Match Gemmini NN helper semantics: no transpose, no full_C/low_D, weightA=0.
  tiled_matmul_auto(dim_I, dim_J, dim_K,
                   (const elem_t *)A->data, (const elem_t *)B->data,
                   /*D=*/nullptr,
                   /*C=*/(void *)C->data,
                   /*stride_A=*/A_stride,
                   /*stride_B=*/B_stride,
                   /*stride_D=*/C_stride,
                   /*stride_C=*/C_stride,
                   /*A_scale_factor=*/MVIN_SCALE_IDENTITY,
                   /*B_scale_factor=*/MVIN_SCALE_IDENTITY,
                   /*D_scale_factor=*/MVIN_SCALE_IDENTITY,
                   /*act=*/NO_ACTIVATION,
                   /*scale=*/ACC_SCALE_IDENTITY,
                   /*bert_scale=*/0,
                   /*repeating_bias=*/false,
                   /*transpose_A=*/false,
                   /*transpose_B=*/false,
                   /*full_C=*/false,
                   /*low_D=*/false,
                   /*weightA=*/0,
                   /*tiled_matmul_type=*/WS);
}

// When `llvm.emit_c_interface` is present, MLIR/LLVM lowering will typically
// emit calls to `_mlir_ciface_<name>`.
//
// In the lowered LLVM dialect you showed, `@tiled_matmul_nn_auto_mlir` is a
// thin wrapper that packs raw arguments into StridedMemRefType and then calls
// this symbol. Provide it so the final link succeeds.
extern "C" void _mlir_ciface_tiled_matmul_nn_auto_mlir(
    StridedMemRefType<elem_t, 2> *A,
    StridedMemRefType<elem_t, 2> *B,
    StridedMemRefType<elem_t, 2> *C) {
  tiled_matmul_nn_auto_mlir(A, B, C);
}

static inline elem_t load4d(const StridedMemRefType<elem_t, 4> *m,
                            int64_t d0, int64_t d1, int64_t d2, int64_t d3) {
  const elem_t *base = (const elem_t *)m->data + m->offset;
  return *(base + d0 * m->strides[0] + d1 * m->strides[1] + d2 * m->strides[2] +
           d3 * m->strides[3]);
}

static inline void store4d(StridedMemRefType<elem_t, 4> *m,
                           int64_t d0, int64_t d1, int64_t d2, int64_t d3,
                           elem_t v) {
  elem_t *base = (elem_t *)m->data + m->offset;
  *(base + d0 * m->strides[0] + d1 * m->strides[1] + d2 * m->strides[2] +
    d3 * m->strides[3]) = v;
}

extern "C" void tiled_conv_2d_nchw_fchw_auto_mlir(
    StridedMemRefType<elem_t, 4> *input_nchw,
    StridedMemRefType<elem_t, 4> *weights_fchw,
    StridedMemRefType<elem_t, 4> *output_nchw,
    int64_t stride_h, int64_t stride_w,
    int64_t dilation_h, int64_t dilation_w) {
  if (!input_nchw || !weights_fchw || !output_nchw) {
    std::fprintf(stderr, "tiled_conv_2d_nchw_fchw_auto_mlir: null memref\n");
    std::abort();
  }

  if (input_nchw->strides[3] != 1 || weights_fchw->strides[3] != 1 ||
      output_nchw->strides[3] != 1) {
    std::fprintf(stderr,
                 "tiled_conv_2d_nchw_fchw_auto_mlir: only unit-stride innermost dimension supported\n");
    std::abort();
  }

  if (stride_h != stride_w) {
    std::fprintf(stderr,
                 "tiled_conv_2d_nchw_fchw_auto_mlir: stride_h (%ld) != stride_w (%ld) not supported\n",
                 (long)stride_h, (long)stride_w);
    std::abort();
  }
  if (dilation_h != dilation_w) {
    std::fprintf(stderr,
                 "tiled_conv_2d_nchw_fchw_auto_mlir: dilation_h (%ld) != dilation_w (%ld) not supported\n",
                 (long)dilation_h, (long)dilation_w);
    std::abort();
  }

  const int batch_size = (int)input_nchw->sizes[0];
  const int in_channels = (int)input_nchw->sizes[1];
  const int in_row_dim = (int)input_nchw->sizes[2];
  const int in_col_dim = (int)input_nchw->sizes[3];

  const int out_channels = (int)weights_fchw->sizes[0];
  const int k_in_channels = (int)weights_fchw->sizes[1];
  const int kernel_h = (int)weights_fchw->sizes[2];
  const int kernel_w = (int)weights_fchw->sizes[3];

  if (k_in_channels != in_channels) {
    std::fprintf(stderr,
                 "tiled_conv_2d_nchw_fchw_auto_mlir: in_channels mismatch: input C=%d, weights C=%d\n",
                 in_channels, k_in_channels);
    std::abort();
  }
  if (kernel_h != kernel_w) {
    std::fprintf(stderr,
                 "tiled_conv_2d_nchw_fchw_auto_mlir: only square kernels supported (got %dx%d)\n",
                 kernel_h, kernel_w);
    std::abort();
  }

  const int out_row_dim = (int)output_nchw->sizes[2];
  const int out_col_dim = (int)output_nchw->sizes[3];
  if ((int)output_nchw->sizes[0] != batch_size ||
      (int)output_nchw->sizes[1] != out_channels) {
    std::fprintf(stderr,
                 "tiled_conv_2d_nchw_fchw_auto_mlir: output shape mismatch\n");
    std::abort();
  }

  // Gemmini tiled_conv_auto expects input in NHWC and weights in HWIO.
  // Convert NCHW/FCHW -> NHWC/HWIO, call tiled_conv_auto, then convert back.
  const size_t in_size = (size_t)batch_size * in_row_dim * in_col_dim * in_channels;
  const size_t w_size = (size_t)kernel_h * kernel_w * in_channels * out_channels;
  const size_t out_size = (size_t)batch_size * out_row_dim * out_col_dim * out_channels;

  std::vector<elem_t> input_nhwc(in_size);
  std::vector<elem_t> weights_hwio(w_size);
  std::vector<elem_t> output_nhwc(out_size);

  // input: NCHW -> NHWC
  for (int n = 0; n < batch_size; ++n) {
    for (int c = 0; c < in_channels; ++c) {
      for (int h = 0; h < in_row_dim; ++h) {
        for (int w = 0; w < in_col_dim; ++w) {
          const size_t dst = (((size_t)n * in_row_dim + h) * in_col_dim + w) *
                                 in_channels +
                             c;
          input_nhwc[dst] = load4d(input_nchw, n, c, h, w);
        }
      }
    }
  }

  // weights: FCHW (O,I,Kh,Kw) -> HWIO (Kh,Kw,I,O)
  for (int oc = 0; oc < out_channels; ++oc) {
    for (int ic = 0; ic < in_channels; ++ic) {
      for (int kh = 0; kh < kernel_h; ++kh) {
        for (int kw = 0; kw < kernel_w; ++kw) {
          const size_t dst = ((((size_t)kh * kernel_w + kw) * in_channels + ic) *
                                  out_channels +
                              oc);
          weights_hwio[dst] = load4d(weights_fchw, oc, ic, kh, kw);
        }
      }
    }
  }

  // Call Gemmini conv. Assume input has already been padded if needed; padding=0.
  tiled_conv_auto(
      /*batch_size=*/batch_size,
      /*in_row_dim=*/in_row_dim,
      /*in_col_dim=*/in_col_dim,
      /*in_channels=*/in_channels,
      /*out_channels=*/out_channels,
      /*out_row_dim=*/out_row_dim,
      /*out_col_dim=*/out_col_dim,
      /*stride=*/(int)stride_h,
      /*input_dilation=*/1,
      /*kernel_dilation=*/(int)dilation_h,
      /*padding=*/0,
      /*kernel_dim=*/kernel_h,
      /*wrot180=*/false,
      /*trans_output_1203=*/false,
      /*trans_input_3120=*/false,
      /*trans_weight_1203=*/false,
      /*trans_weight_0132=*/false,
      /*input=*/(const elem_t *)input_nhwc.data(),
      /*weights=*/(const elem_t *)weights_hwio.data(),
      /*bias=*/nullptr,
      /*output=*/(elem_t *)output_nhwc.data(),
      /*act=*/NO_ACTIVATION,
      /*scale=*/ACC_SCALE_IDENTITY,
      /*pool_size=*/0,
      /*pool_stride=*/0,
      /*pool_padding=*/0,
      /*tiled_conv_type=*/WS);

  // output: NHWC -> NCHW
  for (int n = 0; n < batch_size; ++n) {
    for (int oc = 0; oc < out_channels; ++oc) {
      for (int h = 0; h < out_row_dim; ++h) {
        for (int w = 0; w < out_col_dim; ++w) {
          const size_t src = (((size_t)n * out_row_dim + h) * out_col_dim + w) *
                                 out_channels +
                             oc;
          store4d(output_nchw, n, oc, h, w, output_nhwc[src]);
        }
      }
    }
  }
}

extern "C" void _mlir_ciface_tiled_conv_2d_nchw_fchw_auto_mlir(
    StridedMemRefType<elem_t, 4> *input,
    StridedMemRefType<elem_t, 4> *weights,
    StridedMemRefType<elem_t, 4> *output,
    int64_t stride_h, int64_t stride_w,
    int64_t dilation_h, int64_t dilation_w) {
  tiled_conv_2d_nchw_fchw_auto_mlir(input, weights, output, stride_h, stride_w,
                                   dilation_h, dilation_w);
}
