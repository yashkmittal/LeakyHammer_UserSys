#!/bin/bash

g++ -static -I include -g -Wall -O3 -o ./attack-binaries/mr_latency ./attack-scripts/rowhammer-mr-latency.cc  ./attack-scripts/rowhammer-side.cc  util/m5/build/x86/out/libm5.a 
echo "rowhammer-mr-latency.cc recompiled"