#include <vector>
#include <unordered_map>
#include <limits>
#include <random>
#include <deque>

#include "base/base.h"
#include "dram_controller/controller.h"
#include "dram_controller/plugin.h"

namespace Ramulator {

class DREAM : public IControllerPlugin, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IControllerPlugin, DREAM, "DREAM", "DREAM Rowhammer Mitigation.")

  private:
    IDRAMController* m_ctrl = nullptr;
    IDRAM* m_dram = nullptr;

    int m_clk = -1;

    // DREAM Parameters
    int m_dream_th = -1;
    int m_dream_k = -1;
    int m_reset_period_ns = -1;
    long m_reset_period_clk = -1;
    bool m_is_debug = false;

    int m_DRFMab_req_id = -1;

    int m_rank_level = -1;
    int m_bank_level = -1;
    int m_row_level = -1;

    int m_num_ranks = -1;
    int m_num_banks_per_rank = -1;
    int m_num_rows_per_bank = -1;

    // TUSC table: tracks activations per row group
    // m_tusc[flat_bank_id][row_group_id]
    std::vector<std::unordered_map<int, int>> m_tusc;

  public:
    void init() override { 
      m_dream_th = param<int>("threshold").required();
      m_dream_k = param<int>("gang_size").default_val(32);
      m_reset_period_ns = param<int>("reset_period_ns").default_val(64000000); // 64ms
      m_is_debug = param<bool>("debug").default_val(false);
    };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {
      m_ctrl = cast_parent<IDRAMController>();
      m_dram = m_ctrl->m_dram;

      if (!m_dram->m_commands.contains("DRFMab")) {
        throw ConfigurationError("DREAM is not compatible with the DRAM implementation that does not have DRFMab command!");
      }

      m_reset_period_clk = (long)m_reset_period_ns * 1000 / m_dram->m_timing_vals("tCK_ps");

      m_DRFMab_req_id = m_dram->m_requests("directed-rfm");

      m_rank_level = m_dram->m_levels("rank");
      m_bank_level = m_dram->m_levels("bank");
      m_row_level = m_dram->m_levels("row");

      m_num_ranks = m_dram->get_level_size("rank");
      m_num_banks_per_rank = m_dram->get_level_size("bankgroup") == -1 ? 
                             m_dram->get_level_size("bank") : 
                             m_dram->get_level_size("bankgroup") * m_dram->get_level_size("bank");
      m_num_rows_per_bank = m_dram->get_level_size("row");

      m_tusc.resize(m_num_ranks * m_num_banks_per_rank);
    };

    void update(bool request_found, ReqBuffer::iterator& req_it) override {
      m_clk++;

      if (m_clk % m_reset_period_clk == 0) {
        for (auto& bank_table : m_tusc) {
          bank_table.clear();
        }
      }

      if (request_found) {
        if (m_dram->m_command_meta(req_it->command).is_opening && m_dram->m_command_scopes(req_it->command) == m_row_level) {
          int flat_bank_id = req_it->addr_vec[m_bank_level];
          int accumulated_dimension = 1;
          for (int i = m_bank_level - 1; i >= m_rank_level; i--) {
            accumulated_dimension *= m_dram->m_organization.count[i + 1];
            flat_bank_id += req_it->addr_vec[i] * accumulated_dimension;
          }
          
          int row_id = req_it->addr_vec[m_row_level];
          int group_id = row_id / m_dream_k;

          m_tusc[flat_bank_id][group_id]++;

          if (m_tusc[flat_bank_id][group_id] >= m_dream_th) {
            std::cout << "[Ramulator::DREAM] Threshold reached for bank " << flat_bank_id << " group " << group_id << " at clk " << m_clk << std::endl;
            
            // Issue Directed Refresh (DRFMab)
            Request drfm_req(req_it->addr_vec, m_DRFMab_req_id);
            m_ctrl->priority_send(drfm_req);
            
            // Reset counter for this group in this bank
            m_tusc[flat_bank_id][group_id] = 0;
          }
        }
      }
    }
};

}       // namespace Ramulator
