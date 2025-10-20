#include "base/base.h"
#include "dram_controller/controller.h"
#include "dram_controller/plugin.h"
#include "dram_controller/impl/plugin/device_config/device_config.h"

namespace Ramulator {

class SideChannelHelper : public IControllerPlugin, public Implementation {
    RAMULATOR_REGISTER_IMPLEMENTATION(IControllerPlugin, SideChannelHelper, "SideChannelHelper", "Side-Channel Attack Helper.")

private:
    DeviceConfig m_cfg;

    Clk_t m_clk = 0;

    int m_rfm_req_id = -1;
    int m_no_send = -1;

public:
    void init() override { }

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {
        m_cfg.set_device(cast_parent<IDRAMController>());

        std::vector<int> m_addr_bits;
        int m_num_levels = -1;
        const auto& count = m_cfg.m_dram->m_organization.count;
        m_num_levels = count.size();
        m_addr_bits.resize(m_num_levels);
        for (size_t level = 0; level < m_addr_bits.size(); level++) {
            m_addr_bits[level] = calc_log2(count[level]);
            std::cout << "LEVEL: " << level << " BITS: " << m_addr_bits[level] << std::endl;
        }

        std::cout << "CH LEVEL: " << m_cfg.m_channel_level << std::endl;
        std::cout << "RK LEVEL: " << m_cfg.m_rank_level << std::endl;
        std::cout << "BG LEVEL: " << m_cfg.m_bankgroup_level << std::endl;
        std::cout << "BK LEVEL: " << m_cfg.m_bank_level << std::endl;
        std::cout << "ROW LEVEL: " << m_cfg.m_row_level << std::endl;
        std::cout << "COL LEVEL: " << m_cfg.m_col_level << std::endl;
      
        int tx_bytes = m_cfg.m_dram->m_internal_prefetch_size * m_cfg.m_dram->m_channel_width / 8;
        int tx_bits = calc_log2(tx_bytes);
        std::cout << "TX BITS: " << tx_bits << std::endl;

        m_rfm_req_id = m_cfg.m_dram->m_requests("rfm");
    }

    void update(bool request_found, ReqBuffer::iterator& req_it) override {
        m_clk++;

        if (!request_found) {
            return;
        }

        auto& req = *req_it;
        auto& req_meta = m_cfg.m_dram->m_command_meta(req.command);
        auto& req_scope = m_cfg.m_dram->m_command_scopes(req.command);

        if (req_meta.is_refreshing) {
            bool is_rfm = req.command == m_rfm_req_id;
            std::string message = is_rfm ? "RFM" : "REFRESH";
            std::cout << "[Ramulator2] [" << m_clk << "] " << message << std::endl;
        }
        
        if (!(req_meta.is_opening && req_scope == m_cfg.m_row_level)) {
            return; 
        }

        auto flat_bank_id = m_cfg.get_flat_bank_id(*req_it);
        std::cout << "[Ramulator2] [" << m_clk << "] (0x" << std::hex << req_it->addr << std::dec << ") ";
        std::cout << "Source: " << req_it->source_id << " ";
        std::cout << "Rank: " << req_it->addr_vec[m_cfg.m_rank_level] << " ";
        std::cout << "BGrp: " << req_it->addr_vec[m_cfg.m_bankgroup_level] << " ";
        std::cout << "Bank: " << req_it->addr_vec[m_cfg.m_bank_level] << " ";
        std::cout << "Row: " << req_it->addr_vec[m_cfg.m_row_level] << " ";
        std::cout << "Col: " << req_it->addr_vec[m_cfg.m_col_level] << std::endl;
    }
};

}       // namespace Ramulator
