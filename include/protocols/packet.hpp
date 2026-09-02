#pragma once

#include "common/types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace discan {

// Protocol specific metadata structs
struct WifiMetadata {
    uint16_t frame_control = 0;
    uint8_t type = 0;
    uint8_t subtype = 0;
    std::string bssid;
    std::string source_mac;
    std::string dest_mac;
    std::string ssid;
    bool is_hidden_ssid = false;
    int channel = 0;
    std::string channel_width = "20 MHz";
    std::string operating_mode = "Infrastructure AP (BSS)";
    std::string phy_standard = "802.11n / ac (Wi-Fi 4/5)";
    std::string encryption = "Open"; // Open, WEP, WPA2-PSK, WPA3-SAE, Enterprise
    std::string akm_suite = "None / Open";
    std::string cipher_suite = "None";
    std::string group_cipher = "None";
    std::string rsn_capabilities;
    bool pmf_required = false;
    bool pmf_capable = false;
    uint16_t beacon_interval_tu = 100;
    uint8_t dtim_period = 1;
    uint8_t dtim_count = 0;
    uint64_t hardware_timestamp = 0;
    uint16_t capabilities = 0;
    uint16_t sequence_number = 0;
    std::vector<std::string> supported_rates;
    std::vector<std::string> extended_rates;
    bool has_wps = false;
    std::string wps_version = "2.0";
    std::string wps_state = "Configured";
    bool wps_locked = false;
    std::string vendor_wps_model;
    std::string vendor_wps_device_name;
    std::string wps_manufacturer;
    std::string wps_model_number;
    std::string wps_serial_number;
    std::string wps_uuid;
    std::vector<std::string> vendor_specific_ies;
    bool is_protected = false;
};

struct BleMetadata {
    uint32_t access_address = 0x8E89BED6;
    uint8_t pdu_type = 0;
    std::string pdu_type_name;
    std::string advertiser_mac;
    bool tx_add = false;
    bool rx_add = false;
    std::string complete_local_name;
    int8_t tx_power_level = 0;
    std::vector<std::string> service_uuids;
    std::string manufacturer_name;
    std::string manufacturer_data_hex;
    bool is_ibeacon = false;
    std::string ibeacon_uuid;
    uint16_t ibeacon_major = 0;
    uint16_t ibeacon_minor = 0;
    int8_t ibeacon_tx_power = 0;
    int ble_channel = 37;
};

struct ZigbeeMetadata {
    uint32_t preamble_shr = 0x00000000;
    uint8_t sfd = 0xA7;
    uint8_t frame_length = 0;
    uint16_t frame_control = 0;
    std::string frame_type_name; // Beacon, Data, Ack, MAC Command
    uint8_t seq_number = 0;
    uint16_t dest_pan_id = 0;
    uint16_t src_pan_id = 0;
    std::string dest_addr_str;
    std::string src_addr_str;
    bool is_coordinator = false;
    bool association_permit = false;
    bool security_enabled = false;
    bool pan_id_compressed = false;
    int zigbee_channel = 15;
    
    // NWK layer
    bool has_nwk_header = false;
    uint16_t nwk_frame_control = 0;
    std::string nwk_frame_type_name;
    uint16_t nwk_dest_addr = 0;
    uint16_t nwk_src_addr = 0;
    uint8_t nwk_radius = 0;
    uint8_t nwk_seq_number = 0;
    uint8_t nwk_seq = 0;
    bool is_encrypted = false;
};

struct LoraMetadata {
    uint8_t sync_word = 0x34; // 0x34 LoRaWAN, 0x12 Private
    uint8_t mhdr = 0;
    uint8_t mtype = 0;
    uint8_t major_version = 0;
    std::string mtype_name; // Join-Request, Join-Accept, Unconfirmed Data Up, Confirmed Data Up
    std::string dev_eui_hex;
    std::string app_eui_hex;
    std::string join_eui_hex;
    std::string dev_addr_hex;
    uint16_t dev_nonce = 0;
    uint32_t fcnt = 0;
    uint8_t fport = 0;
    uint8_t fctrl = 0;
    bool adr = false;
    bool ack = false;
    uint32_t mic = 0;
    bool mic_valid = true;
    int spreading_factor = 7;
    int coding_rate = 1;
    double bandwidth_hz = 125000.0;
};

// Universal Decoded Packet
class Packet {
public:
    Packet() : timestamp(std::chrono::system_clock::now()) {}

    uint64_t packet_id = 0;
    std::chrono::system_clock::time_point timestamp;
    ProtocolType protocol = ProtocolType::UNKNOWN;
    float rssi_dbm = -50.0f;
    double center_freq_hz = 2437000000.0;
    std::string source_address;
    std::string destination_address;
    std::string protocol_subtype;
    std::string summary_description;
    
    // Raw binary byte buffer
    std::vector<uint8_t> raw_data;
    
    // Dissected field hierarchy for Deep Inspector
    std::vector<DissectedField> dissection_tree;
    
    // Protocol specific parsed fields
    WifiMetadata wifi;
    BleMetadata ble;
    ZigbeeMetadata zigbee;
    LoraMetadata lora;

    // Helper to format raw data as hex string
    std::string to_hex_string() const {
        std::ostringstream ss;
        for (size_t i = 0; i < raw_data.size(); ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(raw_data[i]);
            if (i + 1 < raw_data.size()) ss << " ";
        }
        return ss.str();
    }
};

using PacketPtr = std::shared_ptr<Packet>;

} // namespace discan
