#pragma once

#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <cstdint>
#include <memory>
#include <complex>

namespace discan {

// Protocol Types
enum class ProtocolType {
    UNKNOWN = 0,
    WIFI,
    BLUETOOTH,
    ZIGBEE,
    LORA
};

inline const char* protocol_to_string(ProtocolType type) {
    switch (type) {
        case ProtocolType::WIFI: return "Wi-Fi (802.11)";
        case ProtocolType::BLUETOOTH: return "Bluetooth (BLE)";
        case ProtocolType::ZIGBEE: return "Zigbee (802.15.4)";
        case ProtocolType::LORA: return "LoRa / LoRaWAN";
        default: return "Unknown";
    }
}

inline const char* protocol_to_short_string(ProtocolType type) {
    switch (type) {
        case ProtocolType::WIFI: return "WIFI";
        case ProtocolType::BLUETOOTH: return "BLE";
        case ProtocolType::ZIGBEE: return "ZIGBEE";
        case ProtocolType::LORA: return "LORA";
        default: return "UNK";
    }
}

// SDR Hardware Device Types
enum class SdrDeviceType {
    SIMULATED = 0,
    HACKRF_ONE,
    USRP_B210
};

inline const char* sdr_device_to_string(SdrDeviceType type) {
    switch (type) {
        case SdrDeviceType::SIMULATED: return "Synthetic Simulator / PCAP";
        case SdrDeviceType::HACKRF_ONE: return "HackRF One";
        case SdrDeviceType::USRP_B210: return "Ettus USRP B210";
        default: return "Unknown SDR";
    }
}

enum class SdrState {
    DISCONNECTED = 0,
    INITIALIZING,
    READY,
    STREAMING,
    ERROR_STATE
};

// Protocol Scanning Modes
enum class ProtocolScanMode {
    WIFI_2G4_PRIMARY = 0,    // Wi-Fi 2.4 GHz Primary (Ch 1, 6, 11)
    WIFI_2G4_ALL,            // Wi-Fi 2.4 GHz All (Ch 1-14)
    WIFI_5G_UNII,            // Wi-Fi 5 GHz UNII-1/2/3 (5.18 - 5.825 GHz)
    BLE_ALL_ADV,             // BLE All Advertising (Ch 37, 38, 39)
    ZIGBEE_ALL_16_CH,        // Zigbee All 16 Channels (Ch 11-26 in 4 Wideband DDC blocks)
    LORA_EU868_WIDE,         // LoRa EU868 & IN865 Wideband (868.0 MHz 4 MSPS zero-hop)
    LORA_US915_ALL,          // LoRa US915 All 64 Channels (8 Sub-bands)
    LORA_433_ISM,            // LoRa 433 MHz ISM
    FULL_SPECTRUM_SWEEP      // Full multi-protocol sweep across all 4 standards
};

inline const char* protocol_scan_mode_to_string(ProtocolScanMode mode) {
    switch (mode) {
        case ProtocolScanMode::WIFI_2G4_PRIMARY: return "Wi-Fi 2.4G (Ch 1, 6, 11)";
        case ProtocolScanMode::WIFI_2G4_ALL:     return "Wi-Fi 2.4G (All Ch 1-14)";
        case ProtocolScanMode::WIFI_5G_UNII:     return "Wi-Fi 5 GHz (UNII 1-3)";
        case ProtocolScanMode::BLE_ALL_ADV:      return "Bluetooth BLE (Ch 37, 38, 39)";
        case ProtocolScanMode::ZIGBEE_ALL_16_CH: return "Zigbee 802.15.4 (All 16 Ch)";
        case ProtocolScanMode::LORA_EU868_WIDE:  return "LoRa EU868 / IN865 Wideband";
        case ProtocolScanMode::LORA_US915_ALL:   return "LoRa US915 (All 64 Ch)";
        case ProtocolScanMode::LORA_433_ISM:     return "LoRa 433 MHz ISM";
        case ProtocolScanMode::FULL_SPECTRUM_SWEEP: return "Full Multi-Protocol Sweep";
        default: return "Custom Mode";
    }
}

// Hop Channel descriptor for multi-band schedules
struct HopChannel {
    std::string name;
    ProtocolType protocol;
    double frequency_hz;
    double sample_rate_sps;
    double bandwidth_hz;
    double recommended_dwell_ms;
    int channel_number = 0;
};

// SDR Configuration parameters
struct SdrConfig {
    SdrDeviceType device_type = SdrDeviceType::SIMULATED;
    ProtocolScanMode scan_mode = ProtocolScanMode::WIFI_2G4_PRIMARY;
    double center_frequency_hz = 2437000000.0; // 2.437 GHz (Wi-Fi Ch 6 default)
    double sample_rate_sps = 20000000.0;       // 20 MSPS
    double bandwidth_hz = 20000000.0;          // 20 MHz
    
    // Gain settings
    double rx_gain_db = 32.0;                  // General Gain (USRP 0-76 dB)
    int hackrf_lna_gain = 32;                  // HackRF LNA (0-40 dB in 8dB steps)
    int hackrf_vga_gain = 30;                  // HackRF VGA (0-62 dB in 2dB steps)
    bool hackrf_amp_enable = false;            // HackRF +14dB preamp
    
    // Channel hopping
    bool auto_hopping_enabled = true;
    double dwell_time_ms = 450.0;              // Current dwell time in ms
    int current_hop_index = 0;
};

// IQ Sample type
using ComplexSample = std::complex<float>;

// Field in a protocol dissection tree
struct DissectedField {
    DissectedField() = default;
    DissectedField(const std::string& n, const std::string& v)
        : name(n), value(v) {}
    DissectedField(const std::string& n, const std::string& v, const std::string& hex, const std::string& desc)
        : name(n), value(v), raw_hex(hex), description(desc) {}

    std::string name;
    std::string value;
    std::string raw_hex;
    std::string description;
    uint32_t bit_offset = 0;
    uint32_t bit_length = 0;
    std::vector<DissectedField> subfields;
    bool is_highlighted = false;
    bool is_unencrypted = true;
};

// Signal strength sample with timestamp
struct RssiSample {
    double timestamp_sec;
    float rssi_dbm;
};

// Anomaly / IDS Alert
enum class AlertSeverity {
    INFO = 0,
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct IdsAlert {
    uint64_t alert_id;
    std::chrono::system_clock::time_point timestamp;
    AlertSeverity severity;
    ProtocolType protocol;
    std::string source_mac;
    std::string title;
    std::string details;
};

} // namespace discan
