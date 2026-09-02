#pragma once

#include "common/types.hpp"
#include "protocols/packet.hpp"
#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace discan {

class Device {
public:
    Device(ProtocolType proto, const std::string& key)
        : protocol(proto),
          device_key(key),
          first_seen(std::chrono::system_clock::now()),
          last_seen(std::chrono::system_clock::now()) {}

    ProtocolType protocol = ProtocolType::UNKNOWN;
    std::string device_key;           // MAC address or DevEUI or Short Address
    std::string display_name;         // SSID or BLE Name or Device Label
    std::string manufacturer;         // Resolved OUI vendor or Bluetooth SIG company
    
    // Timestamps
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    
    // Packet counters & stats
    uint64_t total_packets = 0;
    uint64_t total_bytes = 0;
    float packets_per_second = 0.0f;
    
    // Signal strength
    float current_rssi = -100.0f;
    float min_rssi = 0.0f;
    float max_rssi = -120.0f;
    float avg_rssi = -70.0f;
    std::deque<float> rssi_history;    // 60-sample sparkline history
    static constexpr size_t max_rssi_history = 60;
    
    // Frequency / Channel
    double last_frequency_hz = 0.0;
    int primary_channel = 0;
    
    // Latest full packet for deep inspection
    PacketPtr latest_packet;
    
    // Decoded Protocol Metadata Summary Fields
    std::string extra_info_1;      // e.g. WiFi Encryption / BLE iBeacon / Zigbee PAN ID / LoRa DevAddr
    std::string extra_info_2;      // e.g. WiFi Capabilities / BLE Service UUIDs / Zigbee NWK / LoRa Spreading Factor
    std::string security_info;     // e.g. WPA2-PSK (AES) / Open / Zigbee Enc / LoRa MIC
    std::string preamble_info;     // e.g. 802.11 OFDM Preamble / BLE 0x8E89BED6 / Zigbee SFD 0xA7 / CSS Sync 0x34
    std::string decoded_summary;   // Comprehensive decoded summary for UI display

    void update_with_packet(const PacketPtr& pkt) {
        last_seen = pkt->timestamp;
        total_packets++;
        total_bytes += pkt->raw_data.size();
        last_frequency_hz = pkt->center_freq_hz;
        current_rssi = pkt->rssi_dbm;

        if (total_packets == 1) {
            min_rssi = current_rssi;
            max_rssi = current_rssi;
            avg_rssi = current_rssi;
        } else {
            min_rssi = std::min(min_rssi, current_rssi);
            max_rssi = std::max(max_rssi, current_rssi);
            avg_rssi = avg_rssi * 0.9f + current_rssi * 0.1f;
        }

        rssi_history.push_back(current_rssi);
        if (rssi_history.size() > max_rssi_history) {
            rssi_history.pop_front();
        }

        latest_packet = pkt;

        // Extract metadata based on protocol
        if (protocol == ProtocolType::WIFI) {
            if (!pkt->wifi.ssid.empty()) {
                display_name = pkt->wifi.ssid;
            } else if (display_name.empty()) {
                display_name = "<Hidden / Client>";
            }
            if (pkt->wifi.channel > 0) {
                primary_channel = pkt->wifi.channel;
            }
            extra_info_1 = pkt->wifi.encryption.empty() ? "Open / None" : pkt->wifi.encryption;
            extra_info_2 = "Beacon Int: " + std::to_string(pkt->wifi.beacon_interval_tu) + " TU";
            security_info = extra_info_1;
            preamble_info = "802.11 OFDM/DSSS Preamble + PLCP Header";

            std::ostringstream oss;
            oss << "SSID: " << (display_name.empty() ? "<Hidden>" : display_name)
                << " | Ch: " << primary_channel
                << " | Sec: " << security_info
                << " | Beacon: " << pkt->wifi.beacon_interval_tu << " TU";
            if (!pkt->wifi.vendor_wps_model.empty()) oss << " | WPS: " << pkt->wifi.vendor_wps_model;
            decoded_summary = oss.str();

        } else if (protocol == ProtocolType::BLUETOOTH) {
            // Update manufacturer if discovered in this packet
            if (!pkt->ble.manufacturer_name.empty() && (manufacturer == "Generic / Unknown" || manufacturer == "Unknown" || manufacturer.empty())) {
                manufacturer = pkt->ble.manufacturer_name;
            }

            // Update display name intelligently
            if (!pkt->ble.complete_local_name.empty()) {
                if (display_name.empty() || display_name.rfind("BLE", 0) == 0 || display_name.rfind("[", 0) == 0 || display_name.rfind("ADV_", 0) == 0) {
                    display_name = pkt->ble.complete_local_name;
                }
            } else if (display_name.empty()) {
                if (!manufacturer.empty() && manufacturer != "Generic / Unknown") {
                    display_name = manufacturer + " Accessory";
                } else {
                    display_name = "[RPA] Wireless Device";
                }
            }

            primary_channel = pkt->ble.ble_channel;
            security_info = "BLE Adv (Cleartext Link Layer)";
            preamble_info = "Preamble 0xAA + Access Address 0x8E89BED6";

            std::ostringstream oss;
            oss << "PDU: " << pkt->ble.pdu_type_name << " | Ch: " << primary_channel;
            if (pkt->ble.is_ibeacon) {
                extra_info_1 = "iBeacon (" + std::to_string(pkt->ble.ibeacon_major) + "/" + std::to_string(pkt->ble.ibeacon_minor) + ")";
                oss << " | iBeacon Major: " << pkt->ble.ibeacon_major << " Minor: " << pkt->ble.ibeacon_minor
                    << " (TX: " << static_cast<int>(pkt->ble.ibeacon_tx_power) << " dBm)";
            } else {
                extra_info_1 = pkt->ble.pdu_type_name;
                if (!manufacturer.empty() && manufacturer != "Generic / Unknown") {
                    oss << " | Vendor: " << manufacturer;
                }
            }
            extra_info_2 = (!manufacturer.empty() && manufacturer != "Generic / Unknown") ? manufacturer : "BLE Device";
            decoded_summary = oss.str();

        } else if (protocol == ProtocolType::ZIGBEE) {
            primary_channel = pkt->zigbee.zigbee_channel;
            preamble_info = "SHR (Preamble 0x00000000 + SFD 0xA7) | PHR Len: " + std::to_string(pkt->raw_data.size()) + "B";
            
            std::stringstream pan_ss;
            pan_ss << "0x" << std::hex << std::uppercase << pkt->zigbee.dest_pan_id;
            extra_info_1 = "PAN " + pan_ss.str();
            
            std::string role = pkt->zigbee.is_coordinator ? "Coordinator" : (pkt->zigbee.has_nwk_header ? "Router/Node" : "End Device");
            extra_info_2 = role + (pkt->zigbee.association_permit ? " (Assoc Open)" : "");
            security_info = pkt->zigbee.association_permit ? "Open Association [ALERT]" : "Zigbee 802.15.4 Security";
            
            if (display_name.empty() || display_name.rfind("Zigbee", 0) == 0) {
                display_name = "Zigbee " + role + " (" + device_key + ")";
            }

            std::ostringstream oss;
            oss << "PAN ID: " << pan_ss.str()
                << " | Addr: " << device_key
                << " | Role: " << role
                << " | Seq: " << static_cast<int>(pkt->zigbee.seq_number)
                << (pkt->zigbee.association_permit ? " | [!] Assoc Open" : "");
            decoded_summary = oss.str();

        } else if (protocol == ProtocolType::LORA) {
            std::string dev_id = pkt->lora.dev_addr_hex.empty() ? pkt->lora.dev_eui_hex : pkt->lora.dev_addr_hex;
            preamble_info = "CSS Preamble (8 Up-chirps) | Sync Word: 0x34";
            extra_info_1 = "DevAddr: " + (dev_id.empty() ? device_key : dev_id);
            extra_info_2 = "SF" + std::to_string(pkt->lora.spreading_factor) + " / BW " + std::to_string(static_cast<int>(pkt->lora.bandwidth_hz / 1000)) + "k";
            security_info = "LoRaWAN MAC Header (MType: " + pkt->lora.mtype_name + ")";

            if (display_name.empty() || display_name.rfind("LoRa", 0) == 0) {
                display_name = "LoRa Node (" + (dev_id.empty() ? device_key : dev_id) + ")";
            }

            std::ostringstream oss;
            oss << "LoRaWAN " << pkt->lora.mtype_name
                << " | DevAddr: " << (dev_id.empty() ? device_key : dev_id)
                << " | SF" << pkt->lora.spreading_factor
                << " BW " << static_cast<int>(pkt->lora.bandwidth_hz / 1000) << "kHz"
                << " | FCnt: " << pkt->lora.fcnt
                << " | FPort: " << static_cast<int>(pkt->lora.fport);
            decoded_summary = oss.str();
        }
    }
};

using DevicePtr = std::shared_ptr<Device>;

} // namespace discan
