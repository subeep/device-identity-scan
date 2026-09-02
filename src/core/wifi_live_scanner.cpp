#include "core/wifi_live_scanner.hpp"
#include "protocols/wifi_dissector.hpp"
#include "common/logger.hpp"
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>
#include <vector>
#include <chrono>

namespace discan {

void WifiLiveScanner::start(std::function<void(const PacketPtr&)> packet_cb) {
    if (running_) return;
    packet_callback_ = packet_cb;
    running_ = true;
    worker_thread_ = std::thread(&WifiLiveScanner::scanner_loop, this);
    DISCAN_LOG_INFO("Real-World Wi-Fi Live Scanner started");
}

void WifiLiveScanner::stop() {
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void WifiLiveScanner::scanner_loop() {
    // Initial immediate scan
    scan_once();

    while (running_) {
        // Sleep 2.5 seconds between periodic scans
        for (int i = 0; i < 25 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (running_) {
            scan_once();
        }
    }
}

void WifiLiveScanner::scan_once() {
    // Query Linux NetworkManager / nl80211 for real surrounding Wi-Fi networks
    std::string cmd = "nmcli -t -f SSID,BSSID,CHAN,FREQ,SIGNAL,SECURITY dev wifi list 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[512];
    std::vector<std::string> lines;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        lines.push_back(buffer);
    }
    pclose(pipe);

    for (auto& line : lines) {
        // Remove trailing newline
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        // Format: SSID:BSSID:CHAN:FREQ:SIGNAL:SECURITY
        // Note: colons in BSSID might be escaped e.g. 3C\:52\:A1\:0B\:BF\:D7 or plain
        // Let's parse with backslash-escape support
        std::vector<std::string> fields;
        std::string cur_field;
        bool in_escape = false;

        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];
            if (c == '\\' && !in_escape) {
                in_escape = true;
            } else if (c == ':' && !in_escape) {
                fields.push_back(cur_field);
                cur_field.clear();
            } else {
                cur_field += c;
                in_escape = false;
            }
        }
        fields.push_back(cur_field);

        if (fields.size() < 5) continue;

        std::string ssid = fields[0];
        std::string bssid_str = fields[1];
        std::string chan_str = fields[2];
        std::string freq_str = fields[3];
        std::string signal_str = fields[4];
        std::string sec_str = (fields.size() >= 6) ? fields[5] : "Open";

        // Clean unescaped backslashes in BSSID
        std::string clean_bssid;
        for (char c : bssid_str) {
            if (c != '\\') clean_bssid += c;
        }

        int channel = std::atoi(chan_str.c_str());
        if (channel <= 0) channel = 1;

        double freq_hz = 2412000000.0;
        if (!freq_str.empty()) {
            double freq_mhz = std::atof(freq_str.c_str());
            if (freq_mhz > 100.0) {
                freq_hz = freq_mhz * 1e6;
            }
        }

        int signal_pct = std::atoi(signal_str.c_str());
        float rssi_dbm = (signal_pct / 2.0f) - 100.0f; // 100% -> -50 dBm, 50% -> -75 dBm

        // Parse 6-byte BSSID
        uint8_t bssid[6] = { 0 };
        unsigned int b[6] = { 0 };
        if (std::sscanf(clean_bssid.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
            for (int i = 0; i < 6; ++i) bssid[i] = static_cast<uint8_t>(b[i]);
        } else {
            continue; // Invalid BSSID
        }

        // Construct authentic 802.11 Beacon Frame
        auto pkt = std::make_shared<Packet>();
        pkt->protocol = ProtocolType::WIFI;
        pkt->center_freq_hz = freq_hz;
        pkt->rssi_dbm = rssi_dbm;

        std::vector<uint8_t> bytes = {
            0x80, 0x00, 0x00, 0x00, // Frame Control (Beacon)
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // DA (Broadcast)
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // SA (BSSID)
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // BSSID
            0x00, 0x00, // Seq
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
            0x64, 0x00, // 100 TU
            static_cast<uint8_t>(sec_str == "Open" ? 0x21 : 0x31), 0x04 // Capabilities
        };

        // Tag 0: SSID
        bytes.push_back(0x00);
        bytes.push_back(static_cast<uint8_t>(ssid.length()));
        bytes.insert(bytes.end(), ssid.begin(), ssid.end());

        // Tag 1: Supported Rates
        bytes.push_back(0x01);
        bytes.push_back(0x08);
        bytes.push_back(0x82); bytes.push_back(0x84); bytes.push_back(0x8B); bytes.push_back(0x96);
        bytes.push_back(0x24); bytes.push_back(0x30); bytes.push_back(0x48); bytes.push_back(0x6C);

        // Tag 3: DS Channel
        bytes.push_back(0x03);
        bytes.push_back(0x01);
        bytes.push_back(static_cast<uint8_t>(channel));

        // Tag 48: RSN (WPA2) if encrypted
        if (sec_str != "Open" && !sec_str.empty()) {
            std::vector<uint8_t> rsn_ie = {
                0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00
            };
            bytes.insert(bytes.end(), rsn_ie.begin(), rsn_ie.end());
        }

        pkt->raw_data = bytes;

        if (WifiDissector::dissect(*pkt)) {
            if (packet_callback_) {
                packet_callback_(pkt);
            }
        }
    }
}

} // namespace discan
