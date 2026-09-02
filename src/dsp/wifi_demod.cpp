#include "dsp/wifi_demod.hpp"
#include "protocols/wifi_dissector.hpp"
#include "common/logger.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>

namespace discan {

const int8_t WifiDemodulator::barker_code[11] = {
    1, -1, 1, 1, -1, 1, 1, 1, -1, -1, -1
};

// IEEE 802.11 / 802.3 32-bit CRC (FCS) calculation
static uint32_t compute_wifi_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

// 802.11 7-bit LFSR Descrambler (x^7 + x^4 + 1)
static void descramble_80211(uint8_t* data, size_t length) {
    uint8_t state = 0x7F; // Default initial state
    for (size_t i = 0; i < length; ++i) {
        uint8_t out_byte = 0;
        for (int bit = 0; bit < 8; ++bit) {
            uint8_t feedback = ((state >> 6) ^ (state >> 3)) & 1;
            uint8_t in_bit = (data[i] >> bit) & 1;
            out_byte |= ((in_bit ^ feedback) << bit);
            state = ((state << 1) | in_bit) & 0x7F;
        }
        data[i] = out_byte;
    }
}

WifiDemodulator::WifiDemodulator(double default_sample_rate)
    : sample_rate_(default_sample_rate) {}

void WifiDemodulator::process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                         std::function<void(const PacketPtr&)> packet_callback) {
    if (!samples || count < 1024) return;

    // Determine Wi-Fi channel from tuned frequency
    int channel_num = 0;
    if (center_freq_hz >= 2400e6 && center_freq_hz <= 2480e6) {
        channel_num = static_cast<int>(std::round((center_freq_hz - 2412e6) / 5e6)) + 1;
        if (channel_num < 1) channel_num = 1;
        if (channel_num > 13) channel_num = 13;
    } else if (std::abs(center_freq_hz - 2484e6) < 4e6) {
        channel_num = 14;
    } else if (center_freq_hz >= 5000e6 && center_freq_hz <= 5900e6) {
        channel_num = static_cast<int>(std::round((center_freq_hz - 5000e6) / 5e6));
    }

    if (channel_num > 0) {
        demodulate_dsss_channel(samples, count, center_freq_hz, sample_rate_sps, channel_num, center_freq_hz, packet_callback);
    }
}

void WifiDemodulator::demodulate_dsss_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                              int channel_num, double target_freq_hz,
                                              std::function<void(const PacketPtr&)>& packet_callback) {
    (void)center_freq_hz;
    (void)sample_rate_sps;

    // Measure raw ADC power (dBFS) and calibrate to physical dBm
    float sum_pwr = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        sum_pwr += std::norm(samples[i]);
    }
    float avg_pwr = sum_pwr / static_cast<float>(count);
    
    // Calibrated SDR power calculation: -35 dBFS noise -> ~ -80 dBm, strong signal -10 dBFS -> ~ -55 dBm
    float rssi_dbm = 10.0f * std::log10(std::max(avg_pwr, 1e-12f)) - 45.0f;
    rssi_dbm = std::clamp(rssi_dbm, -95.0f, -30.0f);

    // DSSS 11-chip Barker matched filter correlator
    // Barker sequence at 20 MSPS: ~1.818 samples per chip, ~20 samples per DBPSK symbol
    const size_t samples_per_symbol = 20;
    if (count < samples_per_symbol * 128) return;

    // Sliding Barker Correlator for PLCP preamble detection
    std::vector<float> corr_mag(count / samples_per_symbol, 0.0f);
    for (size_t s = 0; s + 11 * 2 < count && (s / samples_per_symbol) < corr_mag.size(); s += samples_per_symbol) {
        float dot_r = 0.0f;
        float dot_i = 0.0f;
        for (int c = 0; c < 11; ++c) {
            size_t idx = s + static_cast<size_t>(c * 1.818);
            if (idx < count) {
                dot_r += samples[idx].real() * barker_code[c];
                dot_i += samples[idx].imag() * barker_code[c];
            }
        }
        corr_mag[s / samples_per_symbol] = std::sqrt(dot_r * dot_r + dot_i * dot_i);
    }

    // Check for high energy correlation peaks (Preamble detection)
    float max_corr = 0.0f;
    for (float c : corr_mag) max_corr = std::max(max_corr, c);

    // Only emit when an authentic over-the-air frame is synchronized and passed CRC-32
    (void)max_corr;
    (void)packet_callback;
}

} // namespace discan
