#!/bin/bash
# Run all bert_base matmul runners and then run simulator on their renamed bitcode.
# Modeled after transformer_small/run_transformer_sims.sh

# Do not exit on the first failure
# set -e

for src in *_matmul.c; do
#for src in matmul_ff2_matmul.c; do
  exe="${src%.c}"
  bc="${exe}_run_rn.bc"

  if [[ ! -f "./$exe" ]] || [[ ! -f "./$bc" ]]; then
    echo "Warning: $exe or $bc not found. Skipping. (Did you run 'make'?)"
    continue
  fi

  echo "Processing $exe $1 ..."

  work_dir="work_${exe}"
  mkdir -p "$work_dir"

  pushd "$work_dir" > /dev/null

  echo "  Running executable ../$exe $1"
  if ! ../"$exe" "$1"; then
    echo "Error: $exe failed. Continuing to next..."
  fi

  echo "  Running simulation for $bc"
  if ! $BASE_PATH/sim/build/my_program \
      "../$bc" \
      "example.txt" \
      "$BASE_PATH/sim/boom_ws.yml" \
      None None; then
      echo "Error: Simulation for $bc failed. Continuing to next..."
  fi


  popd > /dev/null

  echo "Finished $exe"
  echo "--------------------------------------------------"

done

echo "All tasks completed."
