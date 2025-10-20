#!/bin/bash

g++ -static -I include -g -Wall -O3 -o ./attack-binaries/prac_poc_sender ./attack-scripts/prac-poc-sender.cc  ./attack-scripts/rowhammer-side.cc  util/m5/build/x86/out/libm5.a 
g++ -static -I include -g -Wall -O3 -o ./attack-binaries/prac_poc_receiver ./attack-scripts/prac-poc-receiver.cc  ./attack-scripts/rowhammer-side.cc  util/m5/build/x86/out/libm5.a 
echo "PRAC POC recompiled"