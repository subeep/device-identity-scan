#pragma once

#include "protocols/packet.hpp"
#include <deque>
#include <vector>
#include <mutex>
#include <string>

namespace discan {

class PacketStorage {
public:
    static PacketStorage& instance() {
        static PacketStorage inst;
        return inst;
    }

    void store_packet(const PacketPtr& pkt);
    
    std::vector<PacketPtr> get_recent_packets(size_t count = 500);
    PacketPtr get_packet_by_id(uint64_t id);

    void clear();

    // Exporters
    bool export_to_json(const std::string& filepath);
    bool export_to_pcap(const std::string& filepath);

    size_t total_packets_stored();

private:
    PacketStorage() = default;

    std::mutex mutex_;
    std::deque<PacketPtr> packets_;
    size_t max_packets_ = 5000;
    uint64_t id_counter_ = 1;
};

} // namespace discan
