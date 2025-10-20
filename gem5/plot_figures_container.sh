#!/bin/bash

# This script plots the figures.

# Check if container command argument is provided
if [ $# -eq 0 ]; then
    echo "Error: Container command argument is required"
    echo "Usage: $0 <container_command>"
    exit 1
fi

# get container command from arguments
container_command=$1

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to the script directory (only if not already there)
if [ "$PWD" != "$SCRIPT_DIR" ]; then
    echo "Changing to gem5 directory: $SCRIPT_DIR"
    cd "$SCRIPT_DIR" || { echo "Failed to change directory to gem5"; exit 1; }
fi

echo "Plotting figures 4 and 7"
if ! "$container_command" run --rm -v "$PWD:/app/" leakyhammer_artifact "python3 result-scripts/parse_and_print.py"; then
    echo "Error: Plotting failed"
    exit 1
fi

echo "All figures are in gem5/figures directory."