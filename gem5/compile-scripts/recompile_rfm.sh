#!/bin/bash

g++ -static -I include -g -Wall -O3 -o ./attack-binaries/rfm_receiver ./attack-scripts/rowhammer-rfm-receiver.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
g++ -static -I include -g -Wall -O3 -o ./attack-binaries/rfm_sender ./attack-scripts/rowhammer-rfm-sender.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
# g++ -static -I include -g -Wall -O3 -o mr_noise rowhammer-mr-noise.cc rowhammer-side.cc util/m5/build/x86/out/libm5.a
echo "RFM recompiled"
