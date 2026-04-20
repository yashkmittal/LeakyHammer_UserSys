// Copyright 2015, Google, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define __STDC_FORMAT_MACROS

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "rowhammer-addr.hh"
#include "rowhammer-side.hh"

// DREAM-C v2 receiver: spread-row, rate-based decoder.
//
// Why: the original single-row receiver crossed DREAM's per-gang threshold
// (40 ACTs / 64ms / gang_size=32) on its own, generating self-DRFMab events
// indistinguishable from the sender's. We now allocate ROW_COUNT rows whose
// physical row IDs are spaced GANG_SIZE apart, so each row maps to a distinct
// DCT gang. Round-robin probing dilutes the receiver's per-gang ACT count by
// a factor of ROW_COUNT, ideally dropping it below DREAM's threshold while
// still catching the sender's all-bank DRFMab stalls.
//
// Decoding is rate-based: we count DRFMab-class latency spikes per window and
// classify "1" if count > DREAM_DECODE_THRESH. The original boolean decoder
// (>=1 spike) is too brittle when receiver self-triggers add a baseline rate.
// Spread across enough gangs to keep per-gang ACT count below DREAM threshold
// at the chosen probe rate. Constraint: (800 windows * P probes/window) / N gangs
// must stay below 40 ACTs/64ms / (16ms attack / 64ms) = 10 ACTs/16ms.
// Choice: P=10 probes/window (probe_interval=2us so any 7us stall catches >=3 probes),
// N=1024 gangs -> 8000/1024 = 7.8 ACTs/gang/16ms (under threshold).
#define ROW_COUNT          1024
#define GANG_SIZE          32
#define PROBE_INTERVAL_NS  2000  // 2us spacing -> ~10 probes per 20us window
#define DREAM_DECODE_THRESH 0    // per-window spike count to decode "1" (>0 spikes)

const int NUM_CHANNEL = 1;
const int NUM_RANKS = 2;
const int alloc_size = 64;
DDR5_16Gb_x8 target;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <txn_period> <msg_bytes> <data_pattern>" << std::endl;
        return 1;
    }

    uint32_t dream_txn_period = std::atoi(argv[1]);
    int msg_bytes = std::atoi(argv[2]);
    char data_pattern = std::strtol(argv[3], NULL, 16);
    uint32_t dream_timeout = dream_txn_period - 500;

    srand(0xdead);
    // N_CH, N_RA, CH, RA, BG, BA, RO, CO
    // BG=7, Bank=3 — completely different bank from sender (BG=0, Bank=0).
    // Allocate ROW_COUNT rows at row IDs {0, GANG_SIZE, 2*GANG_SIZE, ...}
    // so each row sits in its own DCT gang (gang_id = row_id / GANG_SIZE).
    target = DDR5_16Gb_x8(NUM_CHANNEL, NUM_RANKS, 0, 1, 7, 3, 0, 0);

    std::vector<char*> row_ptrs(ROW_COUNT, 0);
    for (int i = 0; i < ROW_COUNT; i++) {
        target.row = i * GANG_SIZE;
        row_ptrs[i] = (char*) mmap_atk(alloc_size, target.to_physical());
        assert(row_ptrs[i] != MAP_FAILED);
    }

    std::printf("[DREAM-RECV] v2 spread-row: rows=%d, gang_size=%d, probe_interval=%dns, decode_thresh=%d\n",
                ROW_COUNT, GANG_SIZE, PROBE_INTERVAL_NS, DREAM_DECODE_THRESH); FLUSH();
    std::printf("[DREAM-RECV] Timeout: %d\n", dream_timeout); FLUSH();
    std::vector<bool> message(msg_bytes * 8, false);
    std::vector<int> spike_counts(msg_bytes * 8, 0);

    sleep_until(SYNC_POINT);
    uint64_t next_window = m5_rpns() + dream_txn_period;

    std::printf("[DREAM-RECV] End of first window: %lu\n", next_window); FLUSH();
    int min_sleep_assert = std::numeric_limits<int>::max();
    uint64_t ns1 = m5_rpns();
    for (size_t i = 0; i < message.size(); i++) {
        int n_spikes = dream_receive_count(row_ptrs, dream_timeout, PROBE_INTERVAL_NS);
        spike_counts[i] = n_spikes;
        message[i] = (n_spikes > DREAM_DECODE_THRESH);
        next_window += dream_txn_period;
        min_sleep_assert = std::min<int>(min_sleep_assert, (int)(next_window - m5_rpns()));
        sleep_until(next_window);
    }
    uint64_t ns2 = m5_rpns();
    uint64_t latency = ns2 - ns1;
    std::printf("[DREAM-RECV] MinSleepAssert: %d\n", min_sleep_assert);

    std::printf("[DREAM-RECV] Received in %ld ns\n", latency);
    std::printf("[DREAM-RECV] Binary: ");
    for (bool bit : message) {
        std::printf("%d", (int) bit);
    }
    std::printf("\n"); FLUSH();

    // Per-window spike-count histogram & summary (for tuning DECODE_THRESH).
    int max_count = 0;
    long total_spikes = 0;
    for (int c : spike_counts) {
        if (c > max_count) max_count = c;
        total_spikes += c;
    }
    std::vector<int> hist(max_count + 1, 0);
    for (int c : spike_counts) hist[c]++;
    std::printf("[DREAM-RECV] Spike histogram (count: nWindows): ");
    for (int v = 0; v <= max_count; v++) {
        std::printf("%d:%d ", v, hist[v]);
    }
    std::printf("\n");
    std::printf("[DREAM-RECV] Total spikes: %ld over %zu windows (mean %.2f)\n",
                total_spikes, message.size(),
                (double)total_spikes / (double)message.size());
    FLUSH();

    // Compare against expected pattern.
    std::vector<int> correct(msg_bytes * 8, 0);
    for (int i = 0; i < msg_bytes * 8; i++) {
        int bit_idx = i % 8;
        correct[i] = (data_pattern >> (7 - bit_idx)) & 1;
    }

    int errors = 0;
    for (size_t i = 0; i < message.size(); i++) {
        if ((int)message[i] != correct[i]) {
            errors++;
        }
    }
    std::printf("[DREAM-RECV] Errors: %d / %d\n", errors, msg_bytes * 8);
    std::printf("[DREAM-RECV] Error rate: %f\n", (errors / (float)message.size())); FLUSH();

    return 0;
}
