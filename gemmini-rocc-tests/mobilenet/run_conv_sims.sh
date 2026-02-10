#!/bin/bash
# Remove set -e so the script continues even if one simulation fails
# set -e

# Loop over all conv_*_matmul files (based on the presence of the source file)
#for src in conv_37_matmul.c; do
for src in conv_*.c; do
#for src in conv_dw_11_convdw.c; do
#for src in  fc_53_matmul.c; do
    # Strip the .c extension to get the executable name
    exe="${src%.c}"
    
    # Define the bitcode file name
    bc="${exe}_run_rn.bc"
    
    # Check if the executable and bitcode file exist
    if [[ ! -f "./$exe" ]] || [[ ! -f "./$bc" ]]; then
        echo "Warning: $exe or $1 not found. Skipping. (Did you run 'make'?)"
        continue
    fi
    
    echo "Processing $exe $1 ..."
    
    # Create an independent directory for this case
    work_dir="work_${exe}"
    mkdir -p "$work_dir"
    
    # Enter the directory
    pushd "$work_dir" > /dev/null

    echo $PWD
    
    # 1. Run the executable
    # We call the executable from the parent directory
    echo "  Running executable ./$exe $1"
    if ! ../"$exe" "$1"; then
        echo "Error: ./$exe failed. Continuing to next..."
    fi
    
    # 2. Run the simulation command
    # We reference the .bc file from the parent directory
    echo "  Running simulation for $bc"
    if ! $BASE_PATH/sim/build/my_program \
        "../$bc" \
        "example.txt" \
        "$BASE_PATH/sim/boom_ws.yml" \
        None None; then
        echo "Error: Simulation for $bc failed. Continuing to next..."
    fi
    
    # Return to the previous directory
    popd > /dev/null
    
    echo "Finished $exe"
    echo "--------------------------------------------------"

done

echo "All tasks completed."
