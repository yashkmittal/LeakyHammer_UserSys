#!/bin/bash

g++ -static -I include -g -Wall -Werror -O3 -o ./attack-binaries/mr_noise ./attack-scripts/rowhammer-mr-noise.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a 
echo "Noise generator recompiled"
