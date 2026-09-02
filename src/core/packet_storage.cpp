#include "core/packet_storage.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>

namespace discan {

void PacketStorage::store_packet(const PacketPtr& pkt) {
    if (!pkt) return;
    std::lock_guard<std::mutex> lock(mutex_);
    pkt->packet_id = id_counter_++;
    packets_.push_front(pkt);
    if (packets_.size() > max_packets_) {
        packets_.pop_back();
    }
}

std::vector<PacketPtr> PacketStorage::get_recent_packets(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PacketPtr> result;
    size_t limit = std::min(count, packets_.size());
    result.reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        result.push_back(packets_[i]);
    }
    return result;
}

PacketPtr PacketStorage::get_packet_by_id(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pkt : packets_) {
        if (pkt->packet_id == id) {
            return pkt;
        }
    }
    return nullptr;
}

void PacketStorage::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    packets_.clear();
}

size_t PacketStorage::total_packets_stored() {
    std::lock_guard<std::mutex> lock(mutex_);
    return packets_.size();
}

bool PacketStorage::export_to_json(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "[\n";
    for (size_t i = 0; i < packets_.size(); ++i) {
        const auto& pkt = packets_[i];
        out << "  {\n";
        out << "    \"id\": " << pkt->packet_id << ",\n";
        out << "    \"protocol\": \"" << protocol_to_string(pkt->protocol) << "\",\n";
        out << "    \"subtype\": \"" << pkt->protocol_subtype << "\",\n";
        out << "    \"source\": \"" << pkt->source_address << "\",\n";
        out << "    \"destination\": \"" << pkt->destination_address << "\",\n";
        out << "    \"rssi\": " << pkt->rssi_dbm << ",\n";
        out << "    \"frequency_hz\": " << static_cast<uint64_t>(pkt->center_freq_hz) << ",\n";
        out << "    \"summary\": \"" << pkt->summary_description << "\",\n";
        out << "    \"hex_data\": \"" << pkt->to_hex_string() << "\"\n";
        out << "  }" << (i + 1 < packets_.size() ? "," : "") << "\n";
    }
    out << "]\n";
    return true;
}

// Write standard PCAP file format (LinkType 105 for 802.11 or DLT_USER)
bool PacketStorage::export_to_pcap(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    // PCAP Global Header (24 bytes)
    uint32_t magic_number = 0xa1b2c3d4;
    uint16_t version_major = 2;
    uint16_t version_minor = 4;
    int32_t  thiszone = 0;
    uint32_t sigfigs = 0;
    uint32_t snaplen = 65535;
    uint32_t network = 105; // DLT_IEEE802_11 (or 147 for USER)

    out.write(reinterpret_cast<const char*>(&magic_number), 4);
    out.write(reinterpret_cast<const char*>(&version_major), 2);
    out.write(reinterpret_cast<const char*>(&version_minor), 2);
    out.write(reinterpret_cast<const char*>(&thiszone), 4);
    out.write(reinterpret_cast<const char*>(&sigfigs), 4);
    out.write(reinterpret_cast<const char*>(&snaplen), 4);
    out.write(reinterpret_cast<const char*>(&network), 4);

    for (auto it = packets_.rbegin(); it != packets_.rend(); ++it) {
        const auto& pkt = *it;
        auto dur = pkt->timestamp.time_since_epoch();
        uint32_t ts_sec = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(dur).count());
        uint32_t ts_usec = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(dur).count() % 1000000);
        uint32_t incl_len = static_cast<uint32_t>(pkt->raw_data.size());
        uint32_t orig_len = incl_len;

        out.write(reinterpret_cast<const char*>(&ts_sec), 4);
        out.write(reinterpret_cast<const char*>(&ts_usec), 4);
        out.write(reinterpret_cast<const char*>(&incl_len), 4);
        out.write(reinterpret_cast<const char*>(&orig_len), 4);
        out.write(reinterpret_cast<const char*>(pkt->raw_data.data()), incl_len);
    }

    return true;
}

} // namespace discan
