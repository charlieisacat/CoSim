# transformer-small per-matmul runners

This directory contains microbench runners for each matmul in a "transformer-small" configuration.

## Matmuls covered

- QKV projections: `matmul_qkv_i{0,1,2}_matmul.c` ($M=128, N=512, K=512$)
- Attention score (Q*K) per head (0..3): `matmul_attn_qk_head{0..3}_matmul.c` ($M=128, N=128, K=128$)
- Attention value (attn*V) per head (0..3): `matmul_attn_av_head{0..3}_matmul.c` ($M=128, N=128, K=128$)
- Output projection: `matmul_out_wo_matmul.c` ($M=128, N=512, K=512$)
- FFN1: `matmul_ff1_matmul.c` ($M=128, N=1024, K=512$)
- FFN2: `matmul_ff2_matmul.c` ($M=128, N=512, K=1024$)

Each runner performs a single `tiled_matmul_auto` call (cycle/instret measurement output has been removed).

## Build

Run `make` in this directory to build all runners on the host (LLVM bitcode + rename/instrument passes).

## Run

Use `run_transformer_sims.sh` to run all generated runners and then launch the simulator on their bitcode.
