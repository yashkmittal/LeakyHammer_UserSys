#!/bin/bash

# This script sets up the environment and runs gem5 experiments in parallel using SLURM.

# Check if container command argument is provided
if [ $# -eq 0 ]; then
    echo "Error: Container command argument is required"
    echo "Usage: $0 <container_command>"
    exit 1
fi

# get container command from arguments
container_command=$1

# Ensure we are in the gem5 directory
if [ "$(basename "$PWD")" != "gem5" ]; then
    echo "Changing to gem5 directory"
    cd gem5 || { echo "Failed to change directory to gem5"; exit 1; }
fi


echo "Setting up experiment directories and scripts"
if ! python3 result-scripts/setup_test.py -s -c -cc $container_command; then
    echo "Error: Setup failed"
    exit 1
fi

echo "Running gem5 experiments in parallel"
if ! python3 result-scripts/execute_run_script.py -s; then
    echo "Error: Execution failed"
    exit 1
fi

echo "Experiments scheduled successfully. Now please wait for the results to be generated."