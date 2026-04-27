#!/bin/bash
# DREAM-C covert-channel proof-of-concept: transmit "UTECE".
# Mirrors the structure of the BER matrix runs but with a single experiment.
#
# IMPORTANT: this script lives in gem5/poc-scripts/ rather than gem5/run_scripts/
# because setup_test.py:156 does `rm -r run_scripts` every time it regenerates
# the matrix. Anything we want to survive that nuke needs to live elsewhere.

set -e

HOST_BASE=/home/arshg/dev/LeakyHammer_UserSys/gem5
TMP_ID=tmp_dream_poc_utece
RESULT_TXT=/app/results/dream/poc/dream_poc_utece.txt
OUT_DIR=/app/results/dream/poc/tmpfs/${TMP_ID}

docker image inspect leakyhammer_artifact:latest >/dev/null 2>&1 || \
  docker load --quiet -i /home/arshg/dev/LeakyHammer_UserSys/leakyhammer_artifact.tar

# Step 1 (root): nuke any leftover poc/ from a previous failed run and recreate
# it with the right ownership. Without this, the result of a failed run sits as
# root-owned and the next user-mode container can't write into it.
docker run --rm \
    -v ${HOST_BASE}:/app \
    leakyhammer_artifact \
    "rm -rf /app/results/dream/poc && \
     mkdir -p ${OUT_DIR} && \
     chown -R 1000:1000 /app/results/dream/poc"

# Step 2 (user): actually run gem5. tmp dir is already prepared, so gem5's
# FileSystemConfig.replace_tree() can put its fs/ subtree inside it.
docker run --rm --user 1000:1000 \
    -v ${HOST_BASE}:/app \
    leakyhammer_artifact \
    "/app/build/X86/gem5.opt /app/configs/deprecated/example/se.py \
        --num-cpu=2 --cpu-type=O3CPU --sys-clock=1GHz --cpu-clock=3GHz \
        --mem-type=Ramulator2 --mem-size=32GB \
        --caches --l2cache --num-l2caches=1 \
        --l1d_size=32kB --l1i_size=32kB --l2_size=4MB \
        --l1d_assoc=8 --l1i_assoc=8 --l2_assoc=16 --cacheline_size=64 \
        --ramulator-config=/app/configs/rhsc/ramulator/dream.yaml \
        --m5-outdir=${OUT_DIR} \
        --cmd='/app/attack-binaries/dream_poc_sender;/app/attack-binaries/dream_poc_receiver' \
        --options='20000 5 0x00 UTECE;20000 5 0x00 UTECE' \
        > ${RESULT_TXT} 2>&1"
