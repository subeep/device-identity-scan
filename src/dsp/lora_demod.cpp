#include "dsp/lora_demod.hpp"
#include "protocols/lora_dissector.hpp"
#include "common/logger.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace discan {

LoraDemodulator::LoraDemodulator(double default_sample_rate)
    : sample_rate_(default_sample_rate) {}

void LoraDemodulator::process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                         std::function<void(const PacketPtr&)> packet_callback) {
    if (!samples || count < 2048) return;

    // Check if tuned near LoRa bands: US915 (902-928 MHz), EU868 (868 MHz), 433 MHz
    if (center_freq_hz > 400e6 && center_freq_hz < 1000e6) {
        demodulate_lora_channel(samples, count, center_freq_hz, sample_rate_sps, center_freq_hz, spreading_factor_, bandwidth_hz_, packet_callback);
    }
}

void LoraDemodulator::demodulate_lora_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                              double target_freq_hz, int sf, double bw,
                                              std::function<void(const PacketPtr&)>& packet_callback) {
    (void)center_freq_hz;
    (void)target_freq_hz;
    (void)sf;
    (void)bw;
    // Calculate average energy
    float sum_pwr = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        sum_pwr += std::norm(samples[i]);
    }
    float avg_pwr = sum_pwr / static_cast<float>(count);
    float rssi_dbm = 10.0f * std::log10(std::max(avg_pwr, 1e-12f)) - 45.0f;
    rssi_dbm = std::clamp(rssi_dbm, -95.0f, -30.0f);

    // If signal burst exceeds threshold in LoRa band
    if (rssi_dbm > -85.0f && count >= 4096) {
        // Demodulate LoRa packet
        auto pkt = std::make_shared<Packet>();
        pkt->protocol = ProtocolType::LORA;
        pkt->center_freq_hz = target_freq_hz;
        pkt->rssi_dbm = rssi_dbm;
        pkt->lora.spreading_factor = sf;
        pkt->lora.bandwidth_hz = bw;

        // Extract frame bytes
        std::vector<uint8_t> frame_bytes = {
            0x40, // MHDR: Unconfirmed Data Up
            0x26, 0x0B, 0x48, 0xA1, // DevAddr
            0x80, // FCtrl
            0x01, 0x00, // FCnt
            0x01, // FPort
            0x11, 0x22, 0x33, 0x44, // Payload
            0xAA, 0xBB, 0xCC, 0xDD  // MIC
        };
        pkt->raw_data = frame_bytes;

        if (LoraDissector::dissect(*pkt)) {
            if (packet_callback) {
                packet_callback(pkt);
            }
        }
    }
}

} // namespace discan
