// =============================================================================
// DREAM-C: DRFM-Aware Rowhammer Mitigation (counter-based variant)
// =============================================================================
//
// Faithful implementation of Taneja & Qureshi, "DREAM: Enabling Low-Overhead
// Rowhammer Mitigation via Directed Refresh Management", ISCA '25, §5.
//
// HOW THIS DIFFERS FROM THE EARLIER PER-BANK STUB
// -----------------------------------------------
// The previous version of this file (pre-2026-04-25) kept independent counters
// per (bank, row_group) and increments only ever touched the bank that issued
// the ACT. That contradicts the paper, where DRFMab can mitigate one row from
// each of 32 banks of a sub-channel simultaneously, so a *single shared* DCT
// counter is reused across all 32 banks. The rewrite below brings the plugin
// in line with the paper. See §5.1 of AGENTS.md for the implications for
// LeakyHammer experiments.
//
// MAPPING THE PAPER'S NOMENCLATURE ONTO OUR RAMULATOR2 CONFIG
// -----------------------------------------------------------
// The paper assumes a DDR5 channel containing 2 sub-channels of 32 banks each.
// Our DDR5_16Gb_x8 preset has BG=8 × BA=4 = 32 banks per rank, and the rest
// of the system uses `channel: 1, rank: 2`. So:
//
//     1 sub-channel (in the paper's sense)  ==  1 rank (in our config)
//     32 banks per sub-channel              ==  32 banks per rank
//     128 K rows per bank                   ==  64 K rows per bank (DDR5_16Gb)
//
// This means we maintain one DCT *per rank* (== per sub-channel).
//
// PARAMETERS (see dream.yaml)
// ---------------------------
//   threshold         T_TH from the paper. The paper sets T_TH = T_RH/2 to
//                     handle the "table reset" race safely (§5.3).
//   vertical_sharing  k from the paper §5.5. k=1 -> gang of 32 rows, k=2 -> 64,
//                     k=4 -> 128, k=8 -> 256. The DCT shrinks by k and the
//                     plugin issues k DRFMab back-to-back on a threshold hit.
//   grouping          'random' (default, paper §5.2) -> idx = row XOR mask[bank]
//                     'set_assoc'                    -> idx = row
//   reset_period_ns   tREFW (default 64 ms). Used to compute the spacing of
//                     the gradual reset (paper §5.4: ~16 entries per REF).
//   seed              Determinism for the per-bank XOR masks.
//   debug             Print verbose threshold-hit / config messages.
//   gang_size         Back-compat alias from the old YAML. We just sanity-check
//                     it equals 32 * vertical_sharing and otherwise ignore it.
//
// WHAT WE INTENTIONALLY OMIT
// --------------------------
// Before each DRFMab, the paper has the MC issue 32 ACT+Pre+S commands to
// populate the DAR of every bank. We don't model that explicitly because the
// DDR5-VRR DRAM impl already accounts for the 280 ns rank-wide DRFMab stall,
// which is the only effect a LeakyHammer attacker can observe through latency.
// If you ever need the DAR-populating overhead, inject those ACT+Pre+S
// transactions through `m_ctrl->priority_send(...)` before the DRFMab.
//
// =============================================================================

#include <vector>
#include <random>
#include <string>
#include <algorithm>

#include "base/base.h"
#include "dram_controller/controller.h"
#include "dram_controller/plugin.h"

namespace Ramulator {

class DREAM : public IControllerPlugin, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IControllerPlugin, DREAM, "DREAM",
                                    "DREAM-C Rowhammer Mitigation (cross-bank shared DCT, paper-accurate).")

  private:
    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------
    enum class Grouping { Random, SetAssoc };

    IDRAMController* m_ctrl = nullptr;
    IDRAM*           m_dram = nullptr;

    long m_clk = -1;

    int      m_dream_th         = -1;
    int      m_vertical_sharing = -1;
    int      m_reset_period_ns  = -1;
    Grouping m_grouping         = Grouping::Random;
    int      m_seed             = 42;
    bool     m_is_debug         = false;

    // Derived from config + DRAM organization.
    long m_reset_period_clk    = -1;
    long m_clk_per_entry_reset = -1;
    int  m_DRFMab_req_id       = -1;

    int m_rank_level = -1;
    int m_bank_level = -1;
    int m_row_level  = -1;

    int m_num_ranks          = -1; // each rank == one DDR5 sub-channel
    int m_num_banks_per_rank = -1; // 32 for DDR5 (BG x BA)
    int m_num_rows_per_bank  = -1;
    int m_num_dct_entries    = -1; // = num_rows_per_bank / vertical_sharing

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    // m_dct[rank][index] : the shared DCT counters. One DCT per sub-channel.
    std::vector<std::vector<int>> m_dct;

    // m_xor_mask[rank][bank_within_rank] : random per-bank mask used by the
    // 'random' grouping function to break spatial correlation between RowIDs
    // across banks (paper §5.2, Fig. 13). 32 masks per sub-channel.
    std::vector<std::vector<int>> m_xor_mask;

    // Round-robin reset cursor per sub-channel. We reset one entry per
    // sub-channel every m_clk_per_entry_reset cycles (paper §5.4).
    std::vector<int> m_reset_cursor;
    long             m_last_reset_clk = 0;

  public:
    void init() override {
      m_dream_th         = param<int>("threshold").required();
      m_vertical_sharing = param<int>("vertical_sharing").default_val(1);
      m_reset_period_ns  = param<int>("reset_period_ns").default_val(64000000);
      m_seed             = param<int>("seed").default_val(42);
      m_is_debug         = param<bool>("debug").default_val(false);

      std::string g = param<std::string>("grouping").default_val(std::string("random"));
      if (g == "random" || g == "Random" || g == "RANDOM") {
        m_grouping = Grouping::Random;
      } else if (g == "set_assoc" || g == "SetAssoc" || g == "set-assoc"
                 || g == "set_associative" || g == "SET_ASSOC") {
        m_grouping = Grouping::SetAssoc;
      } else {
        throw ConfigurationError(
            "DREAM: unknown grouping '{}'. Use 'random' or 'set_assoc'.", g);
      }

      // Optional back-compat alias from the old YAML. The paper has
      // gang_size = 32 * vertical_sharing (32 banks per sub-channel,
      // contributing k rows each). We accept either spelling.
      int gang_size = param<int>("gang_size").default_val(-1);
      if (gang_size > 0 && gang_size != 32 * m_vertical_sharing) {
        // Don't hard-error: organisations with != 32 banks per rank exist.
        // Just warn so the user notices.
        std::cout << "[Ramulator::DREAM] WARNING: gang_size=" << gang_size
                  << " disagrees with 32 * vertical_sharing="
                  << (32 * m_vertical_sharing)
                  << ". Trusting vertical_sharing." << std::endl;
      }

      if (m_dream_th <= 0) {
        throw ConfigurationError("DREAM: threshold must be > 0 (got {}).", m_dream_th);
      }
      if (m_vertical_sharing <= 0) {
        throw ConfigurationError("DREAM: vertical_sharing must be > 0 (got {}).",
                                 m_vertical_sharing);
      }
    };

    void setup(IFrontEnd* /*frontend*/, IMemorySystem* /*memory_system*/) override {
      m_ctrl = cast_parent<IDRAMController>();
      m_dram = m_ctrl->m_dram;

      if (!m_dram->m_commands.contains("DRFMab")) {
        throw ConfigurationError(
            "DREAM is not compatible with a DRAM impl that lacks DRFMab. "
            "Use DDR5-VRR or another DDR5 impl that exposes DRFMab.");
      }
      m_DRFMab_req_id = m_dram->m_requests("directed-rfm");

      m_rank_level = m_dram->m_levels("rank");
      m_bank_level = m_dram->m_levels("bank");
      m_row_level  = m_dram->m_levels("row");

      m_num_ranks          = m_dram->get_level_size("rank");
      m_num_banks_per_rank = (m_dram->get_level_size("bankgroup") == -1)
                               ? m_dram->get_level_size("bank")
                               : m_dram->get_level_size("bankgroup")
                                   * m_dram->get_level_size("bank");
      m_num_rows_per_bank  = m_dram->get_level_size("row");

      if (m_num_rows_per_bank % m_vertical_sharing != 0) {
        throw ConfigurationError(
            "DREAM: vertical_sharing ({}) must divide num_rows_per_bank ({}).",
            m_vertical_sharing, m_num_rows_per_bank);
      }
      m_num_dct_entries = m_num_rows_per_bank / m_vertical_sharing;

      // Translate reset window (default 64 ms) into clock cycles, then spread
      // it across the table so that one entry per sub-channel resets every
      // (reset_period_clk / num_entries) cycles. Paper §5.4 calls this out
      // explicitly: "we reset 16 DCT entries (out of 128K) at each REF".
      m_reset_period_clk    = (long) m_reset_period_ns * 1000L
                              / m_dram->m_timing_vals("tCK_ps");
      m_clk_per_entry_reset = std::max(1L, m_reset_period_clk
                                            / (long) m_num_dct_entries);

      m_dct.assign(m_num_ranks, std::vector<int>(m_num_dct_entries, 0));
      m_reset_cursor.assign(m_num_ranks, 0);

      // Initialize per-bank XOR masks. With m_seed fixed by config, this
      // is reproducible across runs of the same experiment.
      //
      // We use std::mt19937_64 (a standardized algorithm with a stable
      // sequence given a seed) and an explicit modulo, INSTEAD of
      // std::uniform_int_distribution. The latter's mapping from rng()
      // output to int is implementation-defined and varies across libstdc++
      // versions, which would break the userspace mask replicator in
      // rowhammer-side.cc::dream_compute_random_masks(). Keep this code in
      // lock-step with the userspace function.
      std::mt19937_64 rng((uint64_t) m_seed);
      m_xor_mask.assign(m_num_ranks, std::vector<int>(m_num_banks_per_rank, 0));
      for (int r = 0; r < m_num_ranks; r++) {
        for (int b = 0; b < m_num_banks_per_rank; b++) {
          m_xor_mask[r][b] = (int) (rng() % (uint64_t) m_num_dct_entries);
        }
      }

      // Always print masks at init (not gated on m_is_debug) so userspace
      // sender/receiver runs can be cross-checked against the simulator's
      // view of the random grouping. Format chosen to be greppable.
      std::cout << "[Ramulator::DREAM] random_masks (seed=" << m_seed
                << ", num_entries=" << m_num_dct_entries << "):" << std::endl;
      for (int r = 0; r < m_num_ranks; r++) {
        std::cout << "[Ramulator::DREAM]   rank=" << r << ":";
        for (int b = 0; b < m_num_banks_per_rank; b++) {
          std::cout << " " << m_xor_mask[r][b];
        }
        std::cout << std::endl;
      }

      if (m_is_debug) {
        std::cout << "[Ramulator::DREAM] config: "
                  << "grouping="
                  << (m_grouping == Grouping::Random ? "random" : "set_assoc")
                  << " threshold=" << m_dream_th
                  << " vertical_sharing=" << m_vertical_sharing
                  << " ranks(=sub-channels)=" << m_num_ranks
                  << " banks_per_subch=" << m_num_banks_per_rank
                  << " dct_entries_per_subch=" << m_num_dct_entries
                  << " reset_period_ns=" << m_reset_period_ns
                  << " clk_per_entry_reset=" << m_clk_per_entry_reset
                  << " seed=" << m_seed
                  << std::endl;
      }
    };

    // Map (rank, bank-within-rank, row) -> DCT index, applying the configured
    // grouping function and folding into the actual table size.
    inline int dct_index(int rank, int bank_in_rank, int row) const {
      int base = (m_grouping == Grouping::Random)
                   ? (row ^ m_xor_mask[rank][bank_in_rank])
                   : row;
      int idx = base % m_num_dct_entries;
      return idx < 0 ? idx + m_num_dct_entries : idx;
    }

    void update(bool request_found, ReqBuffer::iterator& req_it) override {
      m_clk++;

      // ---------------------------------------------------------------------
      // 1. Spread the DCT reset across the refresh window.
      //    `while` (not `if`) because if the simulator skips many cycles
      //    between updates we still want to catch up by resetting multiple
      //    entries -- one per sub-channel per period.
      // ---------------------------------------------------------------------
      while (m_clk - m_last_reset_clk >= m_clk_per_entry_reset) {
        m_last_reset_clk += m_clk_per_entry_reset;
        for (int r = 0; r < m_num_ranks; r++) {
          int& cur = m_reset_cursor[r];
          m_dct[r][cur] = 0;
          cur = (cur + 1) % m_num_dct_entries;
        }
      }

      if (!request_found) return;

      // We only count row-opening commands at row scope (i.e. ACT).
      if (!(m_dram->m_command_meta(req_it->command).is_opening
            && m_dram->m_command_scopes(req_it->command) == m_row_level)) {
        return;
      }

      // ---------------------------------------------------------------------
      // 2. Recover (rank, bank-within-rank, row) from the address vector.
      //    DDR5 levels (in order): channel, rank, bankgroup, bank, row, column.
      //    Bank-within-rank flattens (bankgroup, bank) into 0..(BG*BA-1).
      // ---------------------------------------------------------------------
      int rank = req_it->addr_vec[m_rank_level];

      int bank_in_rank = req_it->addr_vec[m_bank_level];
      int accumulated  = 1;
      for (int i = m_bank_level - 1; i > m_rank_level; i--) {
        accumulated *= m_dram->m_organization.count[i + 1];
        bank_in_rank += req_it->addr_vec[i] * accumulated;
      }

      int row = req_it->addr_vec[m_row_level];
      int idx = dct_index(rank, bank_in_rank, row);

      // ---------------------------------------------------------------------
      // 3. Increment the shared DCT counter and, if we hit T_TH, mitigate.
      // ---------------------------------------------------------------------
      int& counter = m_dct[rank][idx];
      counter++;

      if (counter >= m_dream_th) {
        if (m_is_debug) {
          std::cout << "[Ramulator::DREAM] T_TH hit: rank=" << rank
                    << " bank_in_rank=" << bank_in_rank
                    << " row=" << row
                    << " dct_idx=" << idx
                    << " clk=" << m_clk
                    << " -> issuing " << m_vertical_sharing
                    << " DRFMab" << std::endl;
        }

        // Per paper §5.5 (Vertical Sharing): for a gang of 32k rows we issue
        // k DRFMab commands back-to-back. For the default (k=1) this is just
        // one DRFMab, matching paper §5.4.
        for (int k = 0; k < m_vertical_sharing; k++) {
          Request drfm_req(req_it->addr_vec, m_DRFMab_req_id);
          m_ctrl->priority_send(drfm_req);
        }

        // Paper §5.4: "Once DRFM finishes, MC issues the ACT and sets the
        // counter to 1." (i.e. count the ACT we're servicing right now.)
        counter = 1;
      }
    }
};

}       // namespace Ramulator
