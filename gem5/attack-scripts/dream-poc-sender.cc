// Copyright 2015, Google, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// DREAM-C proof-of-concept covert channel sender.
//
// This is a thin variant of rowhammer-dream-sender.cc that, instead of
// repeating a single hex byte, transmits a literal ASCII string (default
// "UTECE") as MSB-first bits. Receiver counterpart: dream-poc-receiver.cc.
//
// Usage: dream_poc_sender <txn_period> <msg_bytes> <data_pattern> [<message>]
//   - <txn_period>   : window size in ns (use 20000 to match the BER matrix).
//   - <msg_bytes>    : kept for argv compatibility with the BER framework, ignored.
//   - <data_pattern> : same, ignored.
//   - <message>      : OPTIONAL ASCII string to transmit. Defaults to "UTECE".
//
// The receiver derives msg_bytes from its own copy of the expected message,
// so sender and receiver MUST use the same string.

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
#include <string>
#include <limits>

#include "rowhammer-addr.hh"
#include "rowhammer-side.hh"

// Sender maps rows 0 and 1 in BG=0, Bank=0. Both rows share DCT gang 0
// under the cross-bank shared DCT plugin (gang_id = (row_id ^ mask[bank]) %
// num_entries; consecutive rows in the same bank land in adjacent gangs,
// and the receiver's spread-row probe avoids these specific gangs).
// Hammering either row pumps the same DREAM counter; once the counter
// crosses the threshold the controller issues DRFMab, stalling all 32
// banks of the rank for ~280ns -- visible to the receiver from a
// completely different bank.
#define ROW_COUNT   2

const int NUM_CHANNEL = 1;
const int NUM_RANKS = 2;
const int alloc_size = 64;
DDR5_16Gb_x8 target;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0]
                  << " <txn_period> <msg_bytes> <data_pattern> [<message>]"
                  << std::endl;
        return 1;
    }

    uint32_t dream_txn_period = std::atoi(argv[1]);

    // 4th arg overrides the default message.
    std::string message_string = (argc >= 5) ? std::string(argv[4]) : "UTECE";

    uint32_t dream_timeout = dream_txn_period - 500;
    std::printf("[DREAM-POC-SEND] txn_period: %d msg_chars: %zu message: '%s'\n",
                dream_txn_period, message_string.size(), message_string.c_str());
    FLUSH();

    srand(0xbeef);
    // N_CH, N_RA, CH, RA, BG, BA, RO, CO
    target = DDR5_16Gb_x8(NUM_CHANNEL, NUM_RANKS, 0, 1, 0, 0, 0, 0);

    std::vector<char*> row_ptrs(ROW_COUNT, 0);
    for (int i = 0; i < ROW_COUNT; i++) {
        target.row = i;
        row_ptrs[i] = (char*) mmap_atk(alloc_size, target.to_physical());
        assert(row_ptrs[i] != MAP_FAILED);
    }

    std::printf("[DREAM-POC-SEND] Timeout: %d\n", dream_timeout); FLUSH();

    // Encode the ASCII string as MSB-first bits.
    std::vector<bool> message(message_string.size() * 8, 0);
    for (size_t i = 0; i < message_string.size(); i++) {
        char c = message_string[i];
        for (int j = 0; j < 8; j++) {
            message[i * 8 + j] = (c >> (7 - j)) & 1;
        }
    }

    std::printf("[DREAM-POC-SEND] Binary: ");
    for (size_t i = 0; i < message.size(); i++) {
        std::printf("%d", (int) message[i]);
    }
    std::printf("\n"); FLUSH();

    sleep_until(SYNC_POINT);
    uint64_t next_window = m5_rpns() + dream_txn_period;

    std::printf("[DREAM-POC-SEND] End of first window: %lu\n", next_window);
    FLUSH();
    int min_sleep_assert = std::numeric_limits<int>::max();
    for (size_t i = 0; i < message.size(); i++) {
        bool bit = message[i];
        if (bit) {
            // Bit=1: hammer to push the gang counter past T_TH and trigger
            // DRFMab, producing an all-bank stall the receiver can detect.
            dream_send(row_ptrs, dream_timeout);
        }
        // Bit=0: idle through window -- no hammering, no DRFMab, receiver
        // sees a quiet window.
        next_window += dream_txn_period;
        min_sleep_assert = std::min<int>(min_sleep_assert,
                                         (int)(next_window - m5_rpns()));
        sleep_until(next_window);
    }
    std::printf("[DREAM-POC-SEND] MinSleepAssert: %d\n", min_sleep_assert);
    FLUSH();

    return 0;
}
