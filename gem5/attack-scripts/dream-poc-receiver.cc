//
// DREAM-C proof-of-concept covert channel receiver.
//
// Variant of rowhammer-dream-receiver.cc that, instead of comparing each
// received bit against a hex pattern, packs the received bits back into
// ASCII bytes and prints the decoded string.
//
// Usage: dream_poc_receiver <txn_period> <msg_bytes> <data_pattern> [<expected_msg>]
//   - <txn_period>    : window size in ns (must match sender; use 20000).
//   - <msg_bytes>     : kept for argv compatibility, ignored (length is taken
//                       from <expected_msg>).
//   - <data_pattern>  : kept for argv compatibility, ignored.
//   - <expected_msg>  : OPTIONAL. The string the sender is transmitting.
//                       Used to size the receive buffer and to count errors
//                       at the end. Defaults to "UTECE".

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

// Spread-row, rate-based decoder (same parameters as the BER-matrix
// dream_receiver). See rowhammer-dream-receiver.cc for the full rationale.
#define ROW_COUNT          1024
#define GANG_SIZE          32
#define PROBE_INTERVAL_NS  2000
#define DREAM_DECODE_THRESH 0

const int NUM_CHANNEL = 1;
const int NUM_RANKS = 2;
const int alloc_size = 64;
DDR5_16Gb_x8 target;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0]
                  << " <txn_period> <msg_bytes> <data_pattern> [<expected_msg>]"
                  << std::endl;
        return 1;
    }

    uint32_t dream_txn_period = std::atoi(argv[1]);
    std::string expected_msg = (argc >= 5) ? std::string(argv[4]) : "UTECE";
    int msg_bytes = (int) expected_msg.size();

    // Same 8us margin as the BER-matrix receiver.
    uint32_t dream_timeout = dream_txn_period - 8000;

    srand(0xdead);
    // BG=7, Bank=3 -- completely different bank from the sender (BG=0, Bank=0).
    // ROW_COUNT rows at row IDs {0, GANG_SIZE, 2*GANG_SIZE, ...} so each
    // row sits in its own DCT gang -> diluted self-trigger rate.
    target = DDR5_16Gb_x8(NUM_CHANNEL, NUM_RANKS, 0, 1, 7, 3, 0, 0);

    std::vector<char*> row_ptrs(ROW_COUNT, 0);
    for (int i = 0; i < ROW_COUNT; i++) {
        target.row = i * GANG_SIZE;
        row_ptrs[i] = (char*) mmap_atk(alloc_size, target.to_physical());
        assert(row_ptrs[i] != MAP_FAILED);
    }

    std::printf("[DREAM-POC-RECV] expected_msg: '%s' (%d chars, %d bits)\n",
                expected_msg.c_str(), msg_bytes, msg_bytes * 8);
    std::printf("[DREAM-POC-RECV] spread-row: rows=%d gang_size=%d "
                "probe_interval=%dns decode_thresh=%d\n",
                ROW_COUNT, GANG_SIZE, PROBE_INTERVAL_NS, DREAM_DECODE_THRESH);
    std::printf("[DREAM-POC-RECV] Timeout: %d\n", dream_timeout); FLUSH();

    std::vector<bool> message(msg_bytes * 8, false);
    std::vector<int>  spike_counts(msg_bytes * 8, 0);

    sleep_until(SYNC_POINT);
    uint64_t next_window = m5_rpns() + dream_txn_period;

    std::printf("[DREAM-POC-RECV] End of first window: %lu\n", next_window);
    FLUSH();
    int min_sleep_assert = std::numeric_limits<int>::max();
    int n_resyncs = 0;
    int total_skipped_bits = 0;
    uint64_t ns1 = m5_rpns();
    for (size_t i = 0; i < message.size(); i++) {
        int n_spikes = dream_receive_count(row_ptrs, dream_timeout, PROBE_INTERVAL_NS);
        spike_counts[i] = n_spikes;
        message[i] = (n_spikes > DREAM_DECODE_THRESH);
        next_window += dream_txn_period;
        int slack = (int)(next_window - m5_rpns());
        min_sleep_assert = std::min<int>(min_sleep_assert, slack);

        // Phase-resync (same as BER receiver) -- if we overran a window
        // boundary, skip ahead by an integer number of periods so the next
        // iteration is grid-aligned with the sender's bit clock.
        uint64_t now = m5_rpns();
        if (now > next_window) {
            uint64_t behind = now - next_window;
            uint64_t skip = (behind + dream_txn_period - 1) / dream_txn_period;
            for (uint64_t s = 0; s < skip && i + 1 < message.size(); s++) {
                ++i;
                message[i] = false;
                spike_counts[i] = -1;
                ++total_skipped_bits;
            }
            next_window += skip * dream_txn_period;
            ++n_resyncs;
        }
        sleep_until(next_window);
    }
    uint64_t ns2 = m5_rpns();
    uint64_t latency = ns2 - ns1;
    std::printf("[DREAM-POC-RECV] MinSleepAssert: %d\n", min_sleep_assert);
    std::printf("[DREAM-POC-RECV] Resyncs: %d (%d bits skipped)\n",
                n_resyncs, total_skipped_bits);
    std::printf("[DREAM-POC-RECV] Received in %ld ns\n", latency);

    // Print the received binary stream.
    std::printf("[DREAM-POC-RECV] Binary: ");
    for (bool bit : message) {
        std::printf("%d", (int) bit);
    }
    std::printf("\n"); FLUSH();

    // Repack into ASCII bytes (MSB-first, same as sender) and print.
    std::string decoded;
    decoded.resize(msg_bytes);
    for (int i = 0; i < msg_bytes; i++) {
        char c = 0;
        for (int j = 0; j < 8; j++) {
            c = (c << 1) | (int) message[i * 8 + j];
        }
        decoded[i] = c;
    }

    // Build a human-friendly view: replace non-printable bytes with '?' so
    // a slightly-corrupted decode still shows you what came through.
    std::string visible = decoded;
    for (char& c : visible) {
        if (c < 32 || c > 126) c = '?';
    }

    std::printf("[DREAM-POC-RECV] Decoded ASCII: '%s'\n", visible.c_str());
    std::printf("[DREAM-POC-RECV] Decoded hex  : ");
    for (unsigned char c : decoded) {
        std::printf("%02X ", c);
    }
    std::printf("\n");
    std::printf("[DREAM-POC-RECV] Expected ASCII: '%s'\n", expected_msg.c_str());

    // Bit-level error count.
    int errors = 0;
    for (int i = 0; i < msg_bytes; i++) {
        char expected_c = expected_msg[i];
        for (int j = 0; j < 8; j++) {
            int expected_bit = (expected_c >> (7 - j)) & 1;
            if ((int) message[i * 8 + j] != expected_bit) errors++;
        }
    }
    int char_errors = 0;
    for (int i = 0; i < msg_bytes; i++) {
        if (decoded[i] != expected_msg[i]) char_errors++;
    }
    std::printf("[DREAM-POC-RECV] Bit errors:  %d / %d  (%.2f%%)\n",
                errors, msg_bytes * 8,
                100.0 * errors / (msg_bytes * 8));
    std::printf("[DREAM-POC-RECV] Char errors: %d / %d\n",
                char_errors, msg_bytes);
    FLUSH();

    return 0;
}
