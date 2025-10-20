#!/bin/bash

g++ -static -I include -g -Wall -O3 -o ./attack-binaries/rfm_poc_sender ./attack-scripts/rfm-poc-sender.cc  ./attack-scripts/rowhammer-side.cc  util/m5/build/x86/out/libm5.a 
g++ -static -I include -g -Wall -O3 -o ./attack-binaries/rfm_poc_receiver ./attack-scripts/rfm-poc-receiver.cc  ./attack-scripts/rowhammer-side.cc  util/m5/build/x86/out/libm5.a 
echo "RFM POC recompiled"