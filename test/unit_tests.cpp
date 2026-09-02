#include "common/oui_database.hpp"
#include "protocols/wifi_dissector.hpp"
#include "protocols/ble_dissector.hpp"
#include "protocols/zigbee_dissector.hpp"
#include "protocols/lora_dissector.hpp"
#include "core/device_tracker.hpp"
#include "core/ids_engine.hpp"
#include "core/packet_storage.hpp"
#include "dsp/fft_analyzer.hpp"
#include "sdr/simulated_device.hpp"
#include "sdr/sdr_manager.hpp"
#include "common/logger.hpp"

#include <iostream>
#include <cassert>
#include <cstring>

void test_oui_database() {
    std::cout << "[TEST] Testing OUI Database..." << std::endl;
    auto& oui = discan::OuiDatabase::instance();
    assert(oui.lookup_mac("24:0A:C4:11:22:33").find("Espressif") != std::string::npos);
    assert(oui.lookup_mac("00:12:4B:AA:BB:CC").find("Texas Instruments") != std::string::npos);
    assert(oui.lookup_mac("AC:87:A3:12:34:56").find("Apple") != std::string::npos);
    assert(oui.lookup_mac("00:17:88:01:02:03").find("Signify") != std::string::npos);
    std::cout << "  -> OUI Database assertions passed." << std::endl;
}

void test_wifi_dissection() {
    std::cout << "[TEST] Testing Wi-Fi 802.11 Beacon Dissection..." << std::endl;
    discan::SimulatedDevice sim;
    // Test simulator generates authentic Wi-Fi packets
    discan::Packet pkt;
    pkt.raw_data = {
        0x80, 0x00, 0x00, 0x00, // FC: Beacon, Dur
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // DA
        0x00, 0x27, 0x22, 0x11, 0x22, 0x33, // SA (Ubiquiti)
        0x00, 0x27, 0x22, 0x11, 0x22, 0x33, // BSSID
        0x00, 0x08, // Seq 128
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // TS
        0x64, 0x00, // Beacon Int 100 TU
        0x11, 0x04, // Capabilities (ESS + Privacy)
        0x00, 0x09, 'T', 'e', 's', 't', '_', 'A', 'P', '_', '1', // Tag 0: SSID
        0x03, 0x01, 0x06 // Tag 3: Ch 6
    };
    bool ok = discan::WifiDissector::dissect(pkt);
    assert(ok);
    assert(pkt.protocol == discan::ProtocolType::WIFI);
    assert(pkt.wifi.ssid == "Test_AP_1");
    assert(pkt.wifi.channel == 6);
    assert(pkt.wifi.source_mac == "00:27:22:11:22:33");
    assert(!pkt.dissection_tree.empty());
    std::cout << "  -> Wi-Fi Dissector assertions passed (SSID: " << pkt.wifi.ssid << ", Ch: " << pkt.wifi.channel << ")." << std::endl;
}

void test_ble_dissection() {
    std::cout << "[TEST] Testing BLE AdvData & iBeacon Dissection..." << std::endl;
    discan::Packet pkt;
    pkt.raw_data = {
        0xD6, 0xBE, 0x89, 0x8E, // Access Address
        0x02, 0x22,             // ADV_NONCONN_IND, Len 34
        0x98, 0x44, 0x12, 0x8F, 0x6C, 0x40, // AdvA: 40:6C:8F:12:44:98 (Apple)
        0x02, 0x01, 0x06,       // Flags
        0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15, // Apple iBeacon
        0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2, 0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0, // UUID
        0x04, 0x12, // Major 1042
        0x16, 0xB4, // Minor 5812
        0xC5        // Tx Power -59
    };
    bool ok = discan::BleDissector::dissect(pkt);
    assert(ok);
    assert(pkt.protocol == discan::ProtocolType::BLUETOOTH);
    assert(pkt.ble.is_ibeacon);
    assert(pkt.ble.ibeacon_major == 1042);
    assert(pkt.ble.ibeacon_minor == 5812);
    assert(pkt.ble.advertiser_mac == "40:6C:8F:12:44:98");
    std::cout << "  -> BLE Dissector assertions passed (iBeacon Major: " << pkt.ble.ibeacon_major << ", Minor: " << pkt.ble.ibeacon_minor << ")." << std::endl;
}

void test_zigbee_dissection() {
    std::cout << "[TEST] Testing Zigbee 802.15.4 MAC & NWK Dissection..." << std::endl;
    discan::Packet pkt;
    pkt.raw_data = {
        0x61, 0x88, // FC: Data, Intra-PAN, Short Dst/Src
        0x35,       // Seq 53
        0x80, 0x1A, // PAN ID 0x1A80 (Philips Hue)
        0x00, 0x00, // Dst 0x0000
        0x21, 0x4A, // Src 0x4A21
        0x08, 0x02, // NWK FC
        0x00, 0x00, // NWK Dst
        0x21, 0x4A, // NWK Src
        0x0A,       // Radius 10
        0x2A        // NWK Seq 42
    };
    bool ok = discan::ZigbeeDissector::dissect(pkt);
    assert(ok);
    assert(pkt.protocol == discan::ProtocolType::ZIGBEE);
    assert(pkt.zigbee.dest_pan_id == 0x1A80);
    assert(pkt.zigbee.src_addr_str == "0x4A21");
    assert(pkt.zigbee.has_nwk_header);
    std::cout << "  -> Zigbee Dissector assertions passed (PAN: 0x1A80, Src: " << pkt.zigbee.src_addr_str << ")." << std::endl;
}

void test_lora_dissection() {
    std::cout << "[TEST] Testing LoRaWAN Join-Request Dissection..." << std::endl;
    discan::Packet pkt;
    pkt.raw_data = {
        0x00, // MHDR: Join Request
        0x33, 0xC2, 0x9B, 0xA4, 0x01, 0xB6, 0x16, 0x00, // JoinEUI
        0x4F, 0x1A, 0x02, 0xD0, 0x7E, 0xD5, 0xB3, 0x70, // DevEUI: 70:B3:D5:7E:D0:02:1A:4F
        0x12, 0x5A, // DevNonce
        0xEF, 0xBE, 0xAD, 0xDE  // MIC
    };
    bool ok = discan::LoraDissector::dissect(pkt);
    assert(ok);
    assert(pkt.protocol == discan::ProtocolType::LORA);
    assert(pkt.lora.mtype_name == "Join Request");
    assert(pkt.lora.dev_eui_hex == "70:B3:D5:7E:D0:02:1A:4F");
    std::cout << "  -> LoRa Dissector assertions passed (DevEUI: " << pkt.lora.dev_eui_hex << ")." << std::endl;
}

void test_fft_analyzer() {
    std::cout << "[TEST] Testing FFT Analyzer & Waterfall Generator..." << std::endl;
    discan::FftAnalyzer fft(256, 30);
    std::vector<discan::ComplexSample> samps(512);
    for (size_t i = 0; i < samps.size(); ++i) {
        float phase = 2.0f * static_cast<float>(M_PI) * 0.1f * i;
        samps[i] = discan::ComplexSample(std::cos(phase), std::sin(phase));
    }
    fft.process_samples(samps.data(), samps.size(), 2437000000.0, 20000000.0);

    std::vector<float> freqs, pwr;
    float peak_f = 0.0f, peak_p = 0.0f;
    fft.get_spectrum_data(freqs, pwr, peak_f, peak_p);

    assert(freqs.size() == 256);
    assert(pwr.size() == 256);
    assert(peak_p > -80.0f);
    std::cout << "  -> FFT Analyzer assertions passed (Peak: " << peak_f << " MHz, Power: " << peak_p << " dBm)." << std::endl;
}

void test_protocol_scan_modes() {
    std::cout << "[TEST] Testing Protocol Scan Modes & Verified Hopping Schedules..." << std::endl;
    auto& sdr = discan::SdrManager::instance();

    // 1. Wi-Fi Primary
    sdr.set_protocol_scan_mode(discan::ProtocolScanMode::WIFI_2G4_PRIMARY);
    assert(sdr.get_hop_schedule().size() == 3);
    assert(sdr.get_dwell_time_ms() == 450.0);

    // 2. Wi-Fi All 14
    sdr.set_protocol_scan_mode(discan::ProtocolScanMode::WIFI_2G4_ALL);
    assert(sdr.get_hop_schedule().size() == 14);

    // 3. BLE All Advertising
    sdr.set_protocol_scan_mode(discan::ProtocolScanMode::BLE_ALL_ADV);
    assert(sdr.get_hop_schedule().size() == 3);
    assert(sdr.get_dwell_time_ms() == 600.0);

    // 4. Zigbee All 16 Channels in 4 Wideband DDC Blocks
    sdr.set_protocol_scan_mode(discan::ProtocolScanMode::ZIGBEE_ALL_16_CH);
    assert(sdr.get_hop_schedule().size() == 4);
    assert(sdr.get_dwell_time_ms() == 600.0);

    // 5. LoRa EU868 Wideband
    sdr.set_protocol_scan_mode(discan::ProtocolScanMode::LORA_EU868_WIDE);
    assert(sdr.get_hop_schedule().size() == 1);
    assert(sdr.get_dwell_time_ms() == 1500.0);

    // 6. LoRa US915 All 64 channels
    sdr.set_protocol_scan_mode(discan::ProtocolScanMode::LORA_US915_ALL);
    assert(sdr.get_hop_schedule().size() == 8);
    assert(sdr.get_dwell_time_ms() == 1200.0);

    std::cout << "  -> All 6 Protocol Hopping schedules and verified dwell times passed." << std::endl;
}

int main() {
    std::cout << "========================================\n"
              << "  Running Device Identity Scan Test Suite \n"
              << "========================================\n" << std::endl;

    test_oui_database();
    test_wifi_dissection();
    test_ble_dissection();
    test_zigbee_dissection();
    test_lora_dissection();
    test_fft_analyzer();
    test_protocol_scan_modes();

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL UNIT TESTS PASSED SUCCESSFULLY!   " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
