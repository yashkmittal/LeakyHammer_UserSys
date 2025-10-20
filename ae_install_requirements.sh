#!/bin/bash

echo "Installing required packages for building gem5"

sudo apt install build-essential
sudo apt install scons
sudo apt install python3-dev
sudo apt install libprotobuf-dev protobuf-compiler libgoogle-perftools-dev
sudo apt install libboost-all-dev
sudo apt install m4

echo "Installed required packages successfully"

# get command line argument if given for exact versions (true or false)
EXACT_VERSIONS=false
if [ "$1" == "true" ]; then
    EXACT_VERSIONS=true
fi

if [ "$EXACT_VERSIONS" == "true" ]; then
    echo "Installing exact versions of Python packages (for Linux 20.04)"
    python3 -m pip install matplotlib==3.1.2 pandas==1.3.4 seaborn==0.11.2 pyyaml==5.4.1 wget==3.2 scipy==1.3.3 numpy==1.17.4 scons==3.1.2
else
    echo "Installing default versions of Python packages"
    python3 -m pip install matplotlib pandas seaborn pyyaml wget scipy numpy scons
fi
