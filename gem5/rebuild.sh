#!/bin/bash

# This script is used to rebuild ramulator and gem5
# arguments are passed to the script as follows:
# --ramulator: rebuild ramulator
# --gem5: rebuild gem5
# --all: rebuild both ramulator and gem5

rebuild_ramulator() {
    echo "Rebuilding ramulator"
    cd ext/ramulator2/ramulator2
    # check if build directory exists, if not create it
    if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    make -j8
    cp ./ramulator2 ../ramulator2
    echo "Ramulator rebuilt successfully"
    cd ../../../
}

rebuild_gem5() {
    echo "Rebuilding gem5"
    python3 `which scons` build/X86/gem5.opt -j7
    echo "gem5 rebuilt successfully"
    scons -C util/m5 build/x86/out/m5
    echo "m5 util built successfully"
}

clean_all(){
    echo "Cleaning gem5"
    rm -r build
    echo "Cleaning ramulator"
    cd ext/ramulator2/ramulator2/
    rm -r build
    echo "Done"
}

case "$1" in
    --ramulator)
        rebuild_ramulator
        ;;
    --gem5)
        rebuild_gem5
        ;;
    --all)
        rebuild_ramulator
        rebuild_gem5
        ;;
    --clean) 
        clean_all
        ;;
    *)
        echo "Usage: $0 {--ramulator|--gem5|--all}"
        exit 1
        ;;
esac

exit 0