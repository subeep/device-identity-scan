#include "sdr/simulated_device.hpp"
#include "protocols/wifi_dissector.hpp"
#include "protocols/ble_dissector.hpp"
#include "protocols/zigbee_dissector.hpp"
#include "protocols/lora_dissector.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace discan {

SimulatedDevice::SimulatedDevice() {
    status_msg_ = "Simulated SDR & Protocol Engine ready";
}

SimulatedDevice::~SimulatedDevice() {
    close();
}

bool SimulatedDevice::initialize() {
    state_ = SdrState::READY;
    status_msg_ = "Simulated SDR initialized";
    return true;
}

bool SimulatedDevice::start_rx(SdrRxCallback callback) {
    rx_callback_ = callback;
    is_running_ = true;
    state_ = SdrState::STREAMING;
    status_msg_ = "Simulated SDR streaming @ " + std::to_string(current_freq_hz_ / 1e6) + " MHz";

    worker_thread_ = std::thread(&SimulatedDevice::worker_loop, this);
    DISCAN_LOG_INFO("Simulated SDR generator started");
    return true;
}

bool SimulatedDevice::stop_rx() {
    if (is_running_) {
        is_running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        state_ = SdrState::READY;
        status_msg_ = "Simulated SDR stopped";
        DISCAN_LOG_INFO("Simulated SDR generator stopped");
        return true;
    }
    return false;
}

void SimulatedDevice::close() {
    stop_rx();
    state_ = SdrState::DISCONNECTED;
}

bool SimulatedDevice::set_frequency(double freq_hz) {
    current_freq_hz_ = freq_hz;
    if (state_ == SdrState::STREAMING) {
        status_msg_ = "Simulated SDR streaming @ " + std::to_string(current_freq_hz_ / 1e6) + " MHz";
    }
    return true;
}

bool SimulatedDevice::set_sample_rate(double rate_sps) {
    current_rate_sps_ = rate_sps;
    return true;
}

bool SimulatedDevice::set_bandwidth(double bw_hz) {
    current_bw_hz_ = bw_hz;
    return true;
}

bool SimulatedDevice::set_gain(double gain_db) {
    gain_db_ = gain_db;
    return true;
}

bool SimulatedDevice::set_hackrf_gains(int lna_gain, int vga_gain, bool amp_enable) {
    gain_db_ = (lna_gain + vga_gain) * 0.5;
    return true;
}

void SimulatedDevice::worker_loop() {
    const size_t block_size = 2048;
    std::vector<ComplexSample> iq_block(block_size);
    
    std::uniform_real_distribution<float> noise_dist(-0.02f, 0.02f);
    std::uniform_real_distribution<float> burst_dist(0.0f, 1.0f);
    
    float phase = 0.0f;
    auto last_packet_time = std::chrono::steady_clock::now();

    while (is_running_) {
        // 1. Generate realistic RF Spectrum Samples (Noise floor + RF tone peaks)
        float signal_tone_freq = 2.0e6f; // +2 MHz offset tone
        float phase_step = 2.0f * static_cast<float>(M_PI) * signal_tone_freq / static_cast<float>(current_rate_sps_);
        
        bool has_burst = burst_dist(rng_) > 0.4f;
        float burst_amp = has_burst ? 0.45f : 0.05f;

        for (size_t i = 0; i < block_size; ++i) {
            float inoise = noise_dist(rng_);
            float qnoise = noise_dist(rng_);
            
            float isig = burst_amp * std::cos(phase) + inoise;
            float qsig = burst_amp * std::sin(phase) + qnoise;
            phase += phase_step;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;

            iq_block[i] = ComplexSample(isig, qsig);
        }

        if (rx_callback_) {
            rx_callback_(iq_block.data(), block_size, current_freq_hz_, current_rate_sps_);
        }

        // 2. Generate simulated multi-protocol packets every 50-100ms
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_packet_time).count() > 80) {
            last_packet_time = now;
            generate_simulated_traffic();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void SimulatedDevice::generate_simulated_traffic() {
    if (!packet_callback_) return;

    static uint64_t tick = 0;
    tick++;

    // Wi-Fi Packets
    if (tick % 2 == 0) {
        static int wifi_idx = 0;
        wifi_idx = (wifi_idx + 1) % 5;
        PacketPtr p;
        if (wifi_idx == 0) p = generate_wifi_beacon(6, "Starlink-Mesh-78A", "00:27:22:A4:8B:11", "WPA2-PSK");
        else if (wifi_idx == 1) p = generate_wifi_beacon(1, "Office_Corporate_5G", "00:00:0C:58:F3:9C", "WPA2/WPA3-Enterprise");
        else if (wifi_idx == 2) p = generate_wifi_beacon(11, "Tesla_Guest_WiFi", "00:1A:11:44:E2:09", "Open");
        else if (wifi_idx == 3) p = generate_wifi_beacon(6, "Apple_HomeKit_GW", "AC:87:A3:42:66:51", "WPA2-PSK");
        else p = generate_wifi_probe_req("24:0A:C4:11:22:33", "Starlink-Mesh-78A"); // Espressif Smart Sensor probe
        
        if (p) packet_callback_(p);
    }

    // BLE Packets
    if (tick % 3 == 0) {
        static int ble_idx = 0;
        ble_idx = (ble_idx + 1) % 5;
        PacketPtr p;
        if (ble_idx == 0) p = generate_ble_adv(37, "40:6C:8F:12:44:98", "AirTag-Keys", true, 1042, 5812);
        else if (ble_idx == 1) p = generate_ble_adv(38, "EC:1B:BD:99:A1:02", "Nordic_Thingy52_Sens", false);
        else if (ble_idx == 2) p = generate_ble_adv(39, "38:0A:94:55:12:7B", "Galaxy Watch 6 LE", false);
        else if (ble_idx == 3) p = generate_ble_adv(37, "70:9E:29:C4:32:01", "Sony WH-1000XM5", false);
        else p = generate_ble_adv(38, "F4:F5:D8:88:99:00", "Google Fast Pair Node", false);

        if (p) packet_callback_(p);
    }

    // Zigbee Packets
    if (tick % 4 == 0) {
        static int zb_idx = 0;
        zb_idx = (zb_idx + 1) % 4;
        PacketPtr p;
        if (zb_idx == 0) p = generate_zigbee_beacon(15, 0x1A80, "00:17:88:01:00:EC:B5:FA"); // Philips Hue Bridge
        else if (zb_idx == 1) p = generate_zigbee_data(15, 0x1A80, 0x4A21, 0x0000); // Aqara contact sensor to coord
        else if (zb_idx == 2) p = generate_zigbee_data(20, 0xA022, 0x1102, 0x0000); // TI CC2652 router
        else p = generate_zigbee_beacon(11, 0xC099, "CC:86:EC:12:34:56:78:9A"); // Silicon Labs coord

        if (p) packet_callback_(p);
    }

    // LoRa / LoRaWAN Packets
    if (tick % 5 == 0) {
        static int lora_idx = 0;
        static uint16_t lora_fcnt = 100;
        lora_fcnt++;
        lora_idx = (lora_idx + 1) % 3;
        PacketPtr p;
        if (lora_idx == 0) p = generate_lora_join_req("70:B3:D5:7E:D0:02:1A:4F", "00:16:B6:01:A4:9B:C2:33");
        else if (lora_idx == 1) p = generate_lora_data_up("0x260B48A1", lora_fcnt, 7);
        else p = generate_lora_data_up("0x01A39F44", lora_fcnt + 50, 9);

        if (p) packet_callback_(p);
    }
}

PacketPtr SimulatedDevice::generate_wifi_beacon(int channel, const std::string& ssid, const std::string& bssid, const std::string& enc) {
    auto pkt = std::make_shared<Packet>();
    pkt->center_freq_hz = 2412000000.0 + (channel - 1) * 5000000.0;
    pkt->rssi_dbm = -45.0f - static_cast<float>((std::rand() % 25));

    // Construct realistic 802.11 Beacon frame bytes
    std::vector<uint8_t> bytes;
    // Frame Control: Management Beacon (Type 0, Subtype 8) -> 0x80 0x00
    bytes.push_back(0x80);
    bytes.push_back(0x00);
    // Duration: 0x00 0x00
    bytes.push_back(0x00);
    bytes.push_back(0x00);
    // Addr1 (DA: Broadcast)
    for (int i = 0; i < 6; ++i) bytes.push_back(0xFF);
    // Addr2 (SA: BSSID)
    std::string clean_bssid;
    for (char c : bssid) if (std::isxdigit(c)) clean_bssid += c;
    for (size_t i = 0; i < 12 && i + 2 <= clean_bssid.length(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(clean_bssid.substr(i, 2), nullptr, 16)));
    }
    while (bytes.size() < 16) bytes.push_back(0x11);
    // Addr3 (BSSID)
    for (int i = 0; i < 6; ++i) bytes.push_back(bytes[10 + i]);
    // Sequence Control: Seq 128, Frag 0
    bytes.push_back(0x00);
    bytes.push_back(0x08);

    // Fixed Params: Timestamp (8), Beacon Int (2), Cap (2)
    uint64_t ts = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (int i = 0; i < 8; ++i) bytes.push_back(static_cast<uint8_t>((ts >> (i * 8)) & 0xFF));
    bytes.push_back(0x64); bytes.push_back(0x00); // 100 TU
    uint16_t cap = (enc == "Open") ? 0x0401 : 0x0411; // ESS + Privacy
    bytes.push_back(cap & 0xFF); bytes.push_back((cap >> 8) & 0xFF);

    // Tag 0: SSID
    bytes.push_back(0);
    bytes.push_back(static_cast<uint8_t>(ssid.length()));
    for (char c : ssid) bytes.push_back(static_cast<uint8_t>(c));

    // Tag 1: Supported Rates
    bytes.push_back(1);
    bytes.push_back(4);
    bytes.push_back(0x82); bytes.push_back(0x84); bytes.push_back(0x8B); bytes.push_back(0x96); // 1, 2, 5.5, 11 Mbps

    // Tag 3: DS Channel
    bytes.push_back(3);
    bytes.push_back(1);
    bytes.push_back(static_cast<uint8_t>(channel));

    // Tag 48: RSN if encrypted
    if (enc != "Open") {
        bytes.push_back(48);
        bytes.push_back(20);
        bytes.push_back(1); bytes.push_back(0); // Version 1
        for (int r = 0; r < 18; ++r) bytes.push_back(0x02);
    }

    pkt->raw_data = bytes;
    WifiDissector::dissect(*pkt);
    return pkt;
}

PacketPtr SimulatedDevice::generate_wifi_probe_req(const std::string& client_mac, const std::string& target_ssid) {
    auto pkt = std::make_shared<Packet>();
    pkt->center_freq_hz = 2437000000.0;
    pkt->rssi_dbm = -58.0f;

    std::vector<uint8_t> bytes;
    // Frame Control: Probe Request (Type 0, Subtype 4) -> 0x40 0x00
    bytes.push_back(0x40); bytes.push_back(0x00);
    bytes.push_back(0x00); bytes.push_back(0x00);
    // Addr1 (DA: Broadcast)
    for (int i = 0; i < 6; ++i) bytes.push_back(0xFF);
    // Addr2 (SA)
    std::string clean;
    for (char c : client_mac) if (std::isxdigit(c)) clean += c;
    for (size_t i = 0; i < 12 && i + 2 <= clean.length(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(clean.substr(i, 2), nullptr, 16)));
    }
    while (bytes.size() < 16) bytes.push_back(0x22);
    // Addr3 (BSSID: Broadcast)
    for (int i = 0; i < 6; ++i) bytes.push_back(0xFF);
    bytes.push_back(0x20); bytes.push_back(0x00);

    // Tag 0: SSID
    bytes.push_back(0);
    bytes.push_back(static_cast<uint8_t>(target_ssid.length()));
    for (char c : target_ssid) bytes.push_back(static_cast<uint8_t>(c));

    pkt->raw_data = bytes;
    WifiDissector::dissect(*pkt);
    return pkt;
}

PacketPtr SimulatedDevice::generate_ble_adv(int channel, const std::string& mac, const std::string& local_name, bool is_beacon, uint16_t major, uint16_t minor) {
    auto pkt = std::make_shared<Packet>();
    double freq = (channel == 37) ? 2402e6 : ((channel == 38) ? 2426e6 : 2480e6);
    pkt->center_freq_hz = freq;
    pkt->rssi_dbm = -52.0f - static_cast<float>((std::rand() % 30));
    pkt->ble.ble_channel = channel;

    std::vector<uint8_t> bytes;
    // Access Address: 0x8E89BED6
    bytes.push_back(0xD6); bytes.push_back(0xBE); bytes.push_back(0x89); bytes.push_back(0x8E);

    // Header: ADV_IND (0x00) or ADV_NONCONN_IND (0x02)
    bytes.push_back(is_beacon ? 0x02 : 0x00);
    size_t len_pos = bytes.size();
    bytes.push_back(0); // Placeholder length

    // AdvA: 6 bytes LSB first
    std::string clean;
    for (char c : mac) if (std::isxdigit(c)) clean += c;
    std::vector<uint8_t> mac_b;
    for (size_t i = 0; i < 12 && i + 2 <= clean.length(); i += 2) {
        mac_b.push_back(static_cast<uint8_t>(std::stoul(clean.substr(i, 2), nullptr, 16)));
    }
    while (mac_b.size() < 6) mac_b.push_back(0x33);
    for (int i = 5; i >= 0; --i) bytes.push_back(mac_b[i]);

    // AD 1: Flags
    bytes.push_back(2); bytes.push_back(0x01); bytes.push_back(0x06); // General Discoverable + BR/EDR Not Supported

    if (is_beacon) {
        // Apple iBeacon (AD Type 0xFF)
        bytes.push_back(26); // Length
        bytes.push_back(0xFF); // Manufacturer Specific
        bytes.push_back(0x4C); bytes.push_back(0x00); // Apple Company ID
        bytes.push_back(0x02); bytes.push_back(0x15); // iBeacon type & len
        // Proximity UUID (16 bytes)
        for (int u = 0; u < 16; ++u) bytes.push_back(static_cast<uint8_t>(0xE2 + u));
        bytes.push_back((major >> 8) & 0xFF); bytes.push_back(major & 0xFF);
        bytes.push_back((minor >> 8) & 0xFF); bytes.push_back(minor & 0xFF);
        bytes.push_back(static_cast<uint8_t>(-59)); // Measured power
    } else {
        // Complete Local Name
        bytes.push_back(static_cast<uint8_t>(local_name.length() + 1));
        bytes.push_back(0x09);
        for (char c : local_name) bytes.push_back(static_cast<uint8_t>(c));

        // TX Power
        bytes.push_back(2); bytes.push_back(0x0A); bytes.push_back(0xF4); // -12 dBm
    }

    // Update length byte
    bytes[len_pos] = static_cast<uint8_t>(bytes.size() - len_pos - 1);

    pkt->raw_data = bytes;
    BleDissector::dissect(*pkt);
    return pkt;
}

PacketPtr SimulatedDevice::generate_zigbee_beacon(int channel, uint16_t pan_id, const std::string& coord_mac) {
    auto pkt = std::make_shared<Packet>();
    pkt->center_freq_hz = 2405000000.0 + (channel - 11) * 5000000.0;
    pkt->rssi_dbm = -64.0f;
    pkt->zigbee.zigbee_channel = channel;

    std::vector<uint8_t> bytes;
    // Frame Control: Beacon, Src Addr Mode 64b Ext -> 0x8000
    bytes.push_back(0x00); bytes.push_back(0xC0);
    bytes.push_back(0x12); // Sequence
    // Source PAN ID
    bytes.push_back(pan_id & 0xFF); bytes.push_back((pan_id >> 8) & 0xFF);
    // Source Extended MAC (8 bytes)
    std::string clean;
    for (char c : coord_mac) if (std::isxdigit(c)) clean += c;
    for (size_t i = 0; i < 16 && i + 2 <= clean.length(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(clean.substr(i, 2), nullptr, 16)));
    }
    while (bytes.size() < 13) bytes.push_back(0x55);

    // Superframe Spec (Coordinator = 1, Assoc Permit = 1) -> 0xCFFF
    bytes.push_back(0xFF); bytes.push_back(0xCF);

    pkt->raw_data = bytes;
    ZigbeeDissector::dissect(*pkt);
    return pkt;
}

PacketPtr SimulatedDevice::generate_zigbee_data(int channel, uint16_t pan_id, uint16_t src_short, uint16_t dst_short) {
    auto pkt = std::make_shared<Packet>();
    pkt->center_freq_hz = 2405000000.0 + (channel - 11) * 5000000.0;
    pkt->rssi_dbm = -60.0f;
    pkt->zigbee.zigbee_channel = channel;

    std::vector<uint8_t> bytes;
    // Frame Control: Data, Intra-PAN Compressed, Dest 16b, Src 16b -> 0x8861
    bytes.push_back(0x61); bytes.push_back(0x88);
    bytes.push_back(0x35); // Sequence
    // Dest PAN ID
    bytes.push_back(pan_id & 0xFF); bytes.push_back((pan_id >> 8) & 0xFF);
    // Dest Short Addr
    bytes.push_back(dst_short & 0xFF); bytes.push_back((dst_short >> 8) & 0xFF);
    // Src Short Addr
    bytes.push_back(src_short & 0xFF); bytes.push_back((src_short >> 8) & 0xFF);

    // Zigbee NWK Header
    bytes.push_back(0x08); bytes.push_back(0x02); // NWK FC: Data
    bytes.push_back(dst_short & 0xFF); bytes.push_back((dst_short >> 8) & 0xFF);
    bytes.push_back(src_short & 0xFF); bytes.push_back((src_short >> 8) & 0xFF);
    bytes.push_back(10); // Radius
    bytes.push_back(42); // NWK Seq

    // Payload (e.g. Temperature telemetry)
    bytes.push_back(0x18); bytes.push_back(0x01); bytes.push_back(0x0A); bytes.push_back(0x00); bytes.push_back(0x29); bytes.push_back(0x09);

    pkt->raw_data = bytes;
    ZigbeeDissector::dissect(*pkt);
    return pkt;
}

PacketPtr SimulatedDevice::generate_lora_join_req(const std::string& dev_eui, const std::string& join_eui) {
    auto pkt = std::make_shared<Packet>();
    pkt->center_freq_hz = 868100000.0; // EU868 Ch 1 / US915
    pkt->rssi_dbm = -85.0f;
    pkt->lora.spreading_factor = 7;
    pkt->lora.bandwidth_hz = 125000.0;

    std::vector<uint8_t> bytes;
    // MHDR: Join Request (0x00)
    bytes.push_back(0x00);

    // JoinEUI (8 bytes LSB)
    std::string clean_j;
    for (char c : join_eui) if (std::isxdigit(c)) clean_j += c;
    std::vector<uint8_t> j_b;
    for (size_t i = 0; i < 16 && i + 2 <= clean_j.length(); i += 2) j_b.push_back(static_cast<uint8_t>(std::stoul(clean_j.substr(i, 2), nullptr, 16)));
    while (j_b.size() < 8) j_b.push_back(0x11);
    for (int i = 7; i >= 0; --i) bytes.push_back(j_b[i]);

    // DevEUI (8 bytes LSB)
    std::string clean_d;
    for (char c : dev_eui) if (std::isxdigit(c)) clean_d += c;
    std::vector<uint8_t> d_b;
    for (size_t i = 0; i < 16 && i + 2 <= clean_d.length(); i += 2) d_b.push_back(static_cast<uint8_t>(std::stoul(clean_d.substr(i, 2), nullptr, 16)));
    while (d_b.size() < 8) d_b.push_back(0x22);
    for (int i = 7; i >= 0; --i) bytes.push_back(d_b[i]);

    // DevNonce (2 bytes)
    bytes.push_back(0x5A); bytes.push_back(0x12);

    // MIC (4 bytes)
    bytes.push_back(0xDE); bytes.push_back(0xAD); bytes.push_back(0xBE); bytes.push_back(0xEF);

    pkt->raw_data = bytes;
    LoraDissector::dissect(*pkt);
    return pkt;
}

PacketPtr SimulatedDevice::generate_lora_data_up(const std::string& dev_addr, uint16_t fcnt, int sf) {
    auto pkt = std::make_shared<Packet>();
    pkt->center_freq_hz = 915200000.0;
    pkt->rssi_dbm = -78.0f;
    pkt->lora.spreading_factor = sf;
    pkt->lora.bandwidth_hz = 125000.0;

    std::vector<uint8_t> bytes;
    // MHDR: Unconfirmed Data Up (0x40)
    bytes.push_back(0x40);

    // DevAddr (4 bytes LSB)
    std::string clean;
    for (char c : dev_addr) if (std::isxdigit(c)) clean += c;
    std::vector<uint8_t> da_b;
    for (size_t i = 0; i < 8 && i + 2 <= clean.length(); i += 2) da_b.push_back(static_cast<uint8_t>(std::stoul(clean.substr(i, 2), nullptr, 16)));
    while (da_b.size() < 4) da_b.push_back(0x01);
    for (int i = 3; i >= 0; --i) bytes.push_back(da_b[i]);

    // FCtrl: ADR=1, ACK=0, FOpts=0 -> 0x80
    bytes.push_back(0x80);

    // FCnt: 2 bytes LSB
    bytes.push_back(fcnt & 0xFF); bytes.push_back((fcnt >> 8) & 0xFF);

    // FPort: 1
    bytes.push_back(0x01);

    // Encrypted Payload bytes
    bytes.push_back(0xA1); bytes.push_back(0xB2); bytes.push_back(0xC3); bytes.push_back(0xD4);

    // MIC (4 bytes)
    bytes.push_back(0x12); bytes.push_back(0x34); bytes.push_back(0x56); bytes.push_back(0x78);

    pkt->raw_data = bytes;
    LoraDissector::dissect(*pkt);
    return pkt;
}

} // namespace discan
