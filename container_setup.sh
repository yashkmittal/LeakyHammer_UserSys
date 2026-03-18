#!/bin/bash

# This script sets up the dependencies and builds the simulators

# Check if container command argument is provided
if [ $# -eq 0 ]; then
    echo "Error: Container command argument is required"
    echo "Usage: $0 <container_command>"
    exit 1
fi

echo "[INFO] Setting up the environment for LeakyHammer"

echo "[INFO] Installing required packages for python3 via uv"
uv sync

# get container command from arguments
container_command=$1

echo "[INFO] Building the Docker image for LeakyHammer (1-2 minutes)"
"$container_command" build . --no-cache --pull -t leakyhammer_artifact

echo "[INFO] Setting up simulators (~30 minutes)"
"$container_command" run --rm \
  --user "$(id -u):$(id -g)" \
  -v $PWD/gem5/:/app/ leakyhammer_artifact \
  "./rebuild.sh --ramulator"

"$container_command" run --rm \
  --user "$(id -u):$(id -g)" \
  -v $PWD/gem5/:/app/ leakyhammer_artifact \
  "./rebuild.sh --gem5"

echo "[INFO] Retrieving small test results and check if everything is working (<10 minutes)"
"$container_command" run --rm \
  --user "$(id -u):$(id -g)" \
  -v $PWD/gem5:/app \
  leakyhammer_artifact \
  ./get_all_results_local.sh


echo "[INFO] Compiling attack scripts (1-2 minutes)"
"$container_command" run --rm \
  --user "$(id -u):$(id -g)" \
  -v $PWD/gem5/:/app/ leakyhammer_artifact \
  "./compile_attack_scripts.sh"

echo "[INFO] Saving the Docker image to a tar file (1-2 minutes)"
"$container_command" save -o leakyhammer_artifact.tar leakyhammer_artifact

echo "[INFO] Setup completed successfully. You can now run experiments using the provided scripts."