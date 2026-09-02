#include "dsp/ble_demod.hpp"
#include "protocols/ble_dissector.hpp"
#include "common/logger.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace discan {

BleDemodulator::BleDemodulator(double default_sample_rate)
    : sample_rate_(default_sample_rate) {}

void BleDemodulator::dewhiten(uint8_t* data, size_t len, uint8_t channel) {
    uint8_t lfsr = (channel & 0x3F) | 0x40;

    for (size_t i = 0; i < len; ++i) {
        uint8_t out_byte = 0;
        for (int b = 0; b < 8; ++b) {
            uint8_t bit = lfsr & 0x01;
            out_byte |= (bit << b);

            uint8_t feedback = ((lfsr >> 6) ^ (lfsr >> 3)) & 0x01;
            lfsr = ((lfsr << 1) | feedback) & 0x7F;
        }
        data[i] ^= out_byte;
    }
}

bool BleDemodulator::check_crc24(const uint8_t* data, size_t len) {
    if (len < 3) return false;
    size_t payload_len = len - 3;

    uint32_t crc = 0x555555;

    for (size_t i = 0; i < payload_len; ++i) {
        uint8_t byte = data[i];
        for (int b = 0; b < 8; ++b) {
            bool bit = (byte >> b) & 1;
            bool crc_bit = (crc >> 23) & 1;
            crc = (crc << 1) & 0xFFFFFF;
            if (bit ^ crc_bit) {
                crc ^= 0x00065B;
            }
        }
    }

    uint32_t pkt_crc = data[payload_len] | (data[payload_len + 1] << 8) | (data[payload_len + 2] << 16);
    return (crc == pkt_crc);
}

void BleDemodulator::process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                        std::function<void(const PacketPtr&)> packet_callback) {
    if (!samples || count < 2048) return;

    // All 40 BLE channels (3 Primary Advertising + 37 Extended Data channels)
    const struct {
        int ch;
        double freq_hz;
    } ble_all_channels[] = {
        { 37, 2402000000.0 }, // Primary Adv
        { 0,  2404000000.0 }, { 1,  2406000000.0 }, { 2,  2408000000.0 }, { 3,  2410000000.0 },
        { 4,  2412000000.0 }, { 5,  2414000000.0 }, { 6,  2416000000.0 }, { 7,  2418000000.0 },
        { 8,  2420000000.0 }, { 9,  2422000000.0 }, { 10, 2424000000.0 },
        { 38, 2426000000.0 }, // Primary Adv
        { 11, 2428000000.0 }, { 12, 2430000000.0 }, { 13, 2432000000.0 }, { 14, 2434000000.0 },
        { 15, 2436000000.0 }, { 16, 2438000000.0 }, { 17, 2440000000.0 }, { 18, 2442000000.0 },
        { 19, 2444000000.0 }, { 20, 2446000000.0 }, { 21, 2448000000.0 }, { 22, 2450000000.0 },
        { 23, 2452000000.0 }, { 24, 2454000000.0 }, { 25, 2456000000.0 }, { 26, 2458000000.0 },
        { 27, 2460000000.0 }, { 28, 2462000000.0 }, { 29, 2464000000.0 }, { 30, 2466000000.0 },
        { 31, 2468000000.0 }, { 32, 2470000000.0 }, { 33, 2472000000.0 }, { 34, 2474000000.0 },
        { 35, 2476000000.0 }, { 36, 2478000000.0 },
        { 39, 2480000000.0 }  // Primary Adv
    };

    double half_bw = sample_rate_sps / 2.0;

    // Process all BLE channels currently in the receiver passband
    for (const auto& bc : ble_all_channels) {
        double offset_hz = bc.freq_hz - center_freq_hz;
        if (std::abs(offset_hz) + 0.8e6 <= half_bw || std::abs(offset_hz) < 1.0e6) {
            demodulate_ble_channel(samples, count, center_freq_hz, sample_rate_sps, bc.ch, bc.freq_hz, packet_callback);
        }
    }
}

void BleDemodulator::demodulate_ble_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                            int ble_channel_num, double target_freq_hz,
                                            std::function<void(const PacketPtr&)>& packet_callback) {
    double freq_offset_hz = target_freq_hz - center_freq_hz;
    double phase_step = -2.0 * M_PI * freq_offset_hz / sample_rate_sps;

    size_t decim = std::max<size_t>(1, static_cast<size_t>(std::round(sample_rate_sps / 2000000.0)));
    double effective_sps = sample_rate_sps / static_cast<double>(decim);
    double samples_per_bit = effective_sps / 1000000.0; // 1 Mbps BLE

    size_t out_len = count / decim;
    if (out_len < 100) return;

    std::vector<ComplexSample> baseband(out_len);
    double cur_phase = 0.0;

    for (size_t i = 0, j = 0; i < count && j < out_len; i += decim, ++j) {
        float cos_val = static_cast<float>(std::cos(cur_phase));
        float sin_val = static_cast<float>(std::sin(cur_phase));
        cur_phase += phase_step * decim;
        if (cur_phase > 2.0 * M_PI) cur_phase -= 2.0 * M_PI;
        else if (cur_phase < -2.0 * M_PI) cur_phase += 2.0 * M_PI;

        baseband[j] = samples[i] * ComplexSample(cos_val, sin_val);
    }

    // FM Demodulation (discriminator)
    std::vector<float> freq_disc(out_len, 0.0f);
    float avg_power = 0.0f;
    double sum_freq = 0.0;

    for (size_t i = 1; i < out_len; ++i) {
        ComplexSample diff = baseband[i] * std::conj(baseband[i - 1]);
        freq_disc[i] = std::atan2(diff.imag(), diff.real());
        avg_power += std::norm(baseband[i]);
        sum_freq += freq_disc[i];
    }
    avg_power /= static_cast<float>(out_len);
    float mean_cfo = static_cast<float>(sum_freq / static_cast<double>(out_len));
    float rssi_dbm = 10.0f * std::log10(std::max(avg_power, 1e-12f)) - 45.0f;
    rssi_dbm = std::clamp(rssi_dbm, -95.0f, -30.0f);

    // Gate on active RF energy
    if (rssi_dbm < -85.0f) {
        return;
    }

    // Adaptive Bit recovery with Carrier Frequency Offset (CFO) DC block
    std::vector<uint8_t> bits;
    bits.reserve(out_len / 2);

    double clock_phase = 0.0;
    for (size_t i = 1; i < out_len; ++i) {
        clock_phase += 1.0;
        if (clock_phase >= samples_per_bit) {
            clock_phase -= samples_per_bit;
            bits.push_back((freq_disc[i] - mean_cfo) > 0.0f ? 1 : 0);
        }
    }

    if (bits.size() < 80) return;

    // Search for Advertising Access Address (0x8E89BED6 = D6 BE 89 8E)
    const uint8_t aa_bytes[4] = { 0xD6, 0xBE, 0x89, 0x8E };

    for (size_t b = 0; b + 80 <= bits.size(); ++b) {
        int match_errors = 0;
        for (int byte_i = 0; byte_i < 4; ++byte_i) {
            for (int bit_i = 0; bit_i < 8; ++bit_i) {
                uint8_t expected_bit = (aa_bytes[byte_i] >> bit_i) & 1;
                uint8_t actual_bit = bits[b + byte_i * 8 + bit_i];
                if (expected_bit != actual_bit) {
                    match_errors++;
                }
            }
        }

        if (match_errors <= 1) { // High confidence Access Address match
            size_t pdu_bit_start = b + 32;
            std::vector<uint8_t> pdu_bytes;
            pdu_bytes.reserve(42);

            for (size_t byte_idx = 0; pdu_bit_start + (byte_idx + 1) * 8 <= bits.size() && byte_idx < 42; ++byte_idx) {
                uint8_t byte_val = 0;
                for (int bit_i = 0; bit_i < 8; ++bit_i) {
                    byte_val |= (bits[pdu_bit_start + byte_idx * 8 + bit_i] << bit_i);
                }
                pdu_bytes.push_back(byte_val);
            }

            if (pdu_bytes.size() >= 8) {
                dewhiten(pdu_bytes.data(), pdu_bytes.size(), static_cast<uint8_t>(ble_channel_num));

                uint8_t pdu_type = pdu_bytes[0] & 0x0F;
                uint8_t payload_len = pdu_bytes[1] & 0x3F;

                if (pdu_type <= 7 && payload_len >= 6 && payload_len <= 37) {
                    size_t expected_total = 2 + payload_len + 3;

                    if (pdu_bytes.size() >= expected_total) {
                        pdu_bytes.resize(expected_total);

                        // Strict CRC-24 verification
                        if (check_crc24(pdu_bytes.data(), expected_total)) {
                            auto pkt = std::make_shared<Packet>();
                            pkt->protocol = ProtocolType::BLUETOOTH;
                            pkt->center_freq_hz = target_freq_hz;
                            pkt->rssi_dbm = rssi_dbm;
                            pkt->ble.ble_channel = ble_channel_num;

                            std::vector<uint8_t> full_pkt = { 0xD6, 0xBE, 0x89, 0x8E };
                            full_pkt.insert(full_pkt.end(), pdu_bytes.begin(), pdu_bytes.end());
                            pkt->raw_data = full_pkt;

                            if (BleDissector::dissect(*pkt)) {
                                if (packet_callback) {
                                    packet_callback(pkt);
                                }
                                b += expected_total * 8;
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace discan
