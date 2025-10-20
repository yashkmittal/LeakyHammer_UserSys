#!/bin/bash

echo "[INFO] Installing required packages for building gem5"
sh ae_install_requirements.sh

cd gem5

echo "[INFO] Building gem5 with Ramulator support..."
echo "[INFO] Building Ramulator2..."
./rebuild.sh --ramulator

echo "[INFO] Building gem5..."
./rebuild.sh --gem5

echo "[INFO] Retrieving small test results and check if everything is working (<10 minutes)"
sh get_all_results_local.sh

echo "[INFO] Compiling attack scripts (1-2 minutes)"
sh compile_attack_scripts.sh

echo "[INFO] Setup completed successfully. You can now run experiments using the provided scripts."