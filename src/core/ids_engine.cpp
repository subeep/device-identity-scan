#include "core/ids_engine.hpp"

namespace discan {

void IdsEngine::inspect_packet(const PacketPtr& pkt) {
    if (!pkt) return;

    // Rule 1: Open / Unencrypted Wi-Fi Network Detected
    if (pkt->protocol == ProtocolType::WIFI && pkt->wifi.type == 0 && pkt->wifi.subtype == 8) { // Beacon
        if (pkt->wifi.encryption == "Open" && !pkt->wifi.ssid.empty() && pkt->wifi.ssid != "<Hidden SSID>") {
            // Check if already alerted recently
            add_alert(AlertSeverity::LOW, ProtocolType::WIFI, pkt->wifi.source_mac, 
                      "Unencrypted Wi-Fi AP: \"" + pkt->wifi.ssid + "\"",
                      "Access Point broadcasting open unencrypted 802.11 network on channel " + std::to_string(pkt->wifi.channel));
        }
    }

    // Rule 2: Deauthentication / Disassociation Attack Detection
    if (pkt->protocol == ProtocolType::WIFI && pkt->wifi.type == 0 && (pkt->wifi.subtype == 12 || pkt->wifi.subtype == 10)) {
        add_alert(AlertSeverity::HIGH, ProtocolType::WIFI, pkt->source_address,
                  "802.11 Deauth/Disassoc Frame Detected",
                  "Possible Wi-Fi deauthentication attack targeting client " + pkt->destination_address);
    }

    // Rule 3: Zigbee Association Permit Enabled (Security Exposure)
    if (pkt->protocol == ProtocolType::ZIGBEE && pkt->zigbee.is_coordinator && pkt->zigbee.association_permit) {
        add_alert(AlertSeverity::MEDIUM, ProtocolType::ZIGBEE, pkt->source_address,
                  "Zigbee Association Permit Active",
                  "Coordinator on PAN " + std::to_string(pkt->zigbee.dest_pan_id) + " allows unauthenticated device pairing");
    }

    // Rule 4: LoRaWAN Join Request Flood
    if (pkt->protocol == ProtocolType::LORA && pkt->lora.mtype_name == "Join Request") {
        add_alert(AlertSeverity::INFO, ProtocolType::LORA, pkt->lora.dev_eui_hex,
                  "LoRaWAN OTAA Join Request",
                  "Device DevEUI: " + pkt->lora.dev_eui_hex + " requesting OTAA session activation");
    }
}

void IdsEngine::add_alert(AlertSeverity severity, ProtocolType proto, const std::string& src, const std::string& title, const std::string& details) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Simple deduplication: avoid adding duplicate alert within 10 seconds for the same src + title
    auto now = std::chrono::system_clock::now();
    std::string dedupe_key = src + ":" + title;
    auto it = last_seen_map_.find(dedupe_key);
    if (it != last_seen_map_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
        if (elapsed < 15) {
            return;
        }
    }
    last_seen_map_[dedupe_key] = now;

    IdsAlert alert;
    alert.alert_id = alert_counter_++;
    alert.timestamp = now;
    alert.severity = severity;
    alert.protocol = proto;
    alert.source_mac = src;
    alert.title = title;
    alert.details = details;

    alerts_.push_front(alert);
    if (alerts_.size() > max_alerts_) {
        alerts_.pop_back();
    }
}

std::vector<IdsAlert> IdsEngine::get_alerts() {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<IdsAlert>(alerts_.begin(), alerts_.end());
}

void IdsEngine::clear_alerts() {
    std::lock_guard<std::mutex> lock(mutex_);
    alerts_.clear();
    last_seen_map_.clear();
}

} // namespace discan
