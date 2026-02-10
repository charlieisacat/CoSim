# bert_base per-matmul runners

This folder contains standalone per-matmul microbenchmarks extracted from `transformers/transformer.c` for the bert-base configuration:

- `seq_len = 128`
- `hidden_dim = 768`
- `expansion_dim = 3072`
- `num_heads = 12` (`hidden_dim_per_head = 64`)

Each file runs exactly one `tiled_matmul_auto(...)` with the dimensions/strides requested, and prints cycles/instret.

Build from `gemmini-rocc-tests/transformers` after updating its `Makefile` to include these tests.
