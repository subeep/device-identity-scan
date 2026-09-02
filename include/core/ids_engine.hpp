#pragma once

#include "common/types.hpp"
#include "protocols/packet.hpp"
#include <vector>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <string>

namespace discan {

class IdsEngine {
public:
    static IdsEngine& instance() {
        static IdsEngine inst;
        return inst;
    }

    void inspect_packet(const PacketPtr& pkt);
    
    std::vector<IdsAlert> get_alerts();
    void clear_alerts();

private:
    IdsEngine() = default;

    std::mutex mutex_;
    std::deque<IdsAlert> alerts_;
    size_t max_alerts_ = 500;
    uint64_t alert_counter_ = 1;

    // Track state for flood / anomaly detection
    std::unordered_map<std::string, uint32_t> beacon_count_last_sec_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_seen_map_;

    void add_alert(AlertSeverity severity, ProtocolType proto, const std::string& src, const std::string& title, const std::string& details);
};

} // namespace discan
