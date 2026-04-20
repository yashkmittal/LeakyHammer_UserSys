#!/bin/bash
set -e

g++ -static -I include -g -Wall -O3 -o ./attack-binaries/dream_sender ./attack-scripts/rowhammer-dream-sender.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
g++ -static -I include -g -Wall -O3 -o ./attack-binaries/dream_receiver ./attack-scripts/rowhammer-dream-receiver.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
echo "DREAM recompiled"
