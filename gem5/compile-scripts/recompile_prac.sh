#!/bin/bash

g++ -static -I include -g -Wall -O3 -o ./attack-binaries/prac_receiver ./attack-scripts/rowhammer-prac-receiver.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
g++ -static -I include -g -Wall -O3 -o ./attack-binaries/prac_sender ./attack-scripts/rowhammer-prac-sender.cc ./attack-scripts/rowhammer-side.cc util/m5/build/x86/out/libm5.a
echo "PRAC recompiled"
