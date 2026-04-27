#!/bin/bash
set -e

g++ -static -I include -g -Wall -O3 -o ./attack-binaries/dream_poc_sender   ./attack-scripts/dream-poc-sender.cc   ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
g++ -static -I include -g -Wall -O3 -o ./attack-binaries/dream_poc_receiver ./attack-scripts/dream-poc-receiver.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
echo "DREAM POC recompiled"
