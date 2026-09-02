#include "core/device_tracker.hpp"
#include <algorithm>

namespace discan {

void DeviceTracker::process_packet(const PacketPtr& pkt) {
    if (!pkt || pkt->source_address.empty()) {
        return;
    }

    std::string key = pkt->source_address;

    // Filter out broadcast and invalid null MAC addresses
    if (key == "FF:FF:FF:FF:FF:FF" || key == "00:00:00:00:00:00" || key == "0000" || key == "FFFF") {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(key);
    if (it == devices_.end()) {
        auto dev = std::make_shared<Device>(pkt->protocol, key);
        
        // Resolve vendor
        std::string resolved_vendor = OuiDatabase::instance().lookup_mac(key);
        if (resolved_vendor != "Unknown" && resolved_vendor != "Unknown Vendor") {
            dev->manufacturer = resolved_vendor;
        } else if (pkt->protocol == ProtocolType::BLUETOOTH && !pkt->ble.manufacturer_name.empty()) {
            dev->manufacturer = pkt->ble.manufacturer_name;
        } else {
            dev->manufacturer = "Generic / Unknown";
        }

        dev->update_with_packet(pkt);
        devices_[key] = dev;
    } else {
        // Merge into existing device record, preserving stable attributes
        auto& dev = it->second;
        
        // If vendor was unknown but new packet has manufacturer data
        if ((dev->manufacturer == "Generic / Unknown" || dev->manufacturer == "Unknown") && 
            !pkt->ble.manufacturer_name.empty()) {
            dev->manufacturer = pkt->ble.manufacturer_name;
        }

        dev->update_with_packet(pkt);
    }
}

std::vector<DevicePtr> DeviceTracker::get_devices() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DevicePtr> result;
    result.reserve(devices_.size());
    for (const auto& pair : devices_) {
        result.push_back(pair.second);
    }
    return result;
}

DevicePtr DeviceTracker::get_device(const std::string& device_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(device_key);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

void DeviceTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    devices_.clear();
}

size_t DeviceTracker::total_device_count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.size();
}

size_t DeviceTracker::count_by_protocol(ProtocolType proto) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& pair : devices_) {
        if (pair.second->protocol == proto) {
            count++;
        }
    }
    return count;
}

} // namespace discan
