#pragma once

#include "core/device.hpp"
#include "protocols/packet.hpp"
#include "common/oui_database.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>
#include <memory>

namespace discan {

class DeviceTracker {
public:
    static DeviceTracker& instance() {
        static DeviceTracker inst;
        return inst;
    }

    void process_packet(const PacketPtr& pkt);

    // Retrieve all tracked devices (thread-safe copy)
    std::vector<DevicePtr> get_devices();
    
    // Retrieve a single device by key
    DevicePtr get_device(const std::string& device_key);

    // Clear all tracked devices
    void clear();

    // Statistics
    size_t total_device_count();
    size_t count_by_protocol(ProtocolType proto);

private:
    DeviceTracker() = default;

    std::mutex mutex_;
    std::unordered_map<std::string, DevicePtr> devices_;
};

} // namespace discan
