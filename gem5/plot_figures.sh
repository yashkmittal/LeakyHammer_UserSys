#!/bin/bash

# This script plots the figures.


# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to the script directory (only if not already there)
if [ "$PWD" != "$SCRIPT_DIR" ]; then
    echo "Changing to gem5 directory: $SCRIPT_DIR"
    cd "$SCRIPT_DIR" || { echo "Failed to change directory to gem5"; exit 1; }
fi


echo "Plotting figures 4 and 7"
if ! uv run python3 result-scripts/parse_and_print.py; then
    echo "Error: Plotting failed"
    exit 1
fi

echo "All figures are in gem5/figures directory."