#include "dsp/zigbee_demod.hpp"
#include "protocols/zigbee_dissector.hpp"
#include "common/logger.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace discan {

// 16 Standard IEEE 802.15.4 32-chip sequences (2.4 GHz PHY)
const uint32_t ZigbeeDemodulator::chip_sequences_32[16] = {
    0x744AC39B, // Symbol 0: 11011001 11000011 01010010 00101110
    0x44AC39B7, // Symbol 1: 11101101 10011100 00110101 00100010
    0x4AC39B74, // Symbol 2: 00101110 11011001 11000011 01010010
    0xAC39B744, // Symbol 3: 00100010 11101101 10011100 00110101
    0xC39B744A, // Symbol 4: 01010010 00101110 11011001 11000011
    0x39B744AC, // Symbol 5: 00110101 00100010 11101101 10011100
    0x9B744AC3, // Symbol 6: 11000011 01010010 00101110 11011001
    0xB744AC39, // Symbol 7: 10011100 00110101 00100010 11101101
    0x8BB53C64, // Symbol 8: Inverse pattern
    0xBB53C648, // Symbol 9
    0xB53C648B, // Symbol 10
    0x53C648BB, // Symbol 11
    0x3C648BB5, // Symbol 12
    0xC648BB53, // Symbol 13
    0x648BB53C, // Symbol 14
    0x48BB53C6  // Symbol 15
};

ZigbeeDemodulator::ZigbeeDemodulator(double default_sample_rate)
    : sample_rate_(default_sample_rate) {}

uint8_t ZigbeeDemodulator::correlate_chip_sequence(uint32_t chip_word, int& out_match_score) {
    uint8_t best_symbol = 0;
    int max_agreements = -1;

    for (uint8_t s = 0; s < 16; ++s) {
        uint32_t diff = chip_word ^ chip_sequences_32[s];
        int errors = __builtin_popcount(diff);
        int agreements = 32 - errors;

        if (agreements > max_agreements) {
            max_agreements = agreements;
            best_symbol = s;
        }
    }

    out_match_score = max_agreements;
    return best_symbol;
}

bool ZigbeeDemodulator::check_crc16(const uint8_t* data, size_t len) {
    if (len < 2) return false;
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len - 2; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1) crc = (crc >> 1) ^ 0x8408;
            else crc >>= 1;
        }
    }
    uint16_t pkt_crc = data[len - 2] | (data[len - 1] << 8);
    return (crc == pkt_crc);
}

void ZigbeeDemodulator::process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                           std::function<void(const PacketPtr&)> packet_callback) {
    if (!samples || count < 2048) return;

    // 802.15.4 2.4 GHz Channels 11-26 (2405 MHz - 2480 MHz, 5 MHz spacing)
    const int channels[] = { 11, 15, 20, 25 };
    double half_bw = sample_rate_sps / 2.0;

    for (int ch : channels) {
        double freq_hz = 2405000000.0 + (ch - 11) * 5000000.0;
        double offset_hz = freq_hz - center_freq_hz;
        if (std::abs(offset_hz) + 1.5e6 <= half_bw) {
            demodulate_zigbee_channel(samples, count, center_freq_hz, sample_rate_sps, ch, freq_hz, packet_callback);
        }
    }
}

void ZigbeeDemodulator::demodulate_zigbee_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                                  int zb_channel_num, double target_freq_hz,
                                                  std::function<void(const PacketPtr&)>& packet_callback) {
    double freq_offset_hz = target_freq_hz - center_freq_hz;
    double phase_step = -2.0 * M_PI * freq_offset_hz / sample_rate_sps;

    // Decimate to ~4 MSPS (2 samples per 2 Mchip/s chip)
    size_t decim = std::max<size_t>(1, static_cast<size_t>(std::round(sample_rate_sps / 4000000.0)));
    double effective_sps = sample_rate_sps / static_cast<double>(decim);
    double samples_per_chip = effective_sps / 2000000.0;

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

    // Demodulate differential phase for chip stream
    std::vector<uint8_t> chips;
    chips.reserve(out_len);

    double chip_clock = 0.0;
    float avg_pwr = 0.0f;
    for (size_t i = 1; i < out_len; ++i) {
        ComplexSample diff = baseband[i] * std::conj(baseband[i - 1]);
        avg_pwr += std::norm(baseband[i]);
        chip_clock += 1.0;
        if (chip_clock >= samples_per_chip) {
            chip_clock -= samples_per_chip;
            chips.push_back(diff.imag() > 0.0f ? 1 : 0);
        }
    }
    avg_pwr /= static_cast<float>(out_len);
    float rssi_dbm = 10.0f * std::log10(std::max(avg_pwr, 1e-12f)) + 12.0f;

    if (chips.size() < 128) return;

    // Search for Preamble + SFD (SFD = 0xA7 -> symbols 0x7 then 0xA in nibbles)
    for (size_t c = 0; c + 128 <= chips.size(); ++c) {
        uint32_t chip_word = 0;
        for (int k = 0; k < 32; ++k) {
            chip_word |= (static_cast<uint32_t>(chips[c + k]) << k);
        }

        int score = 0;
        uint8_t sym = correlate_chip_sequence(chip_word, score);

        if (score >= 26 && sym == 0x0) { // Preamble symbol 0
            // Look ahead for SFD symbol (0x7)
            size_t sfd_pos = c + 32;
            if (sfd_pos + 64 > chips.size()) continue;

            uint32_t sfd_word1 = 0, sfd_word2 = 0;
            for (int k = 0; k < 32; ++k) {
                sfd_word1 |= (static_cast<uint32_t>(chips[sfd_pos + k]) << k);
                sfd_word2 |= (static_cast<uint32_t>(chips[sfd_pos + 32 + k]) << k);
            }

            int sc1 = 0, sc2 = 0;
            uint8_t sfd_sym1 = correlate_chip_sequence(sfd_word1, sc1);
            uint8_t sfd_sym2 = correlate_chip_sequence(sfd_word2, sc2);

            if (sc1 >= 24 && sc2 >= 24 && sfd_sym1 == 0x7 && sfd_sym2 == 0xA) { // SFD 0xA7 matched!
                size_t phr_pos = sfd_pos + 64;
                std::vector<uint8_t> frame_bytes;
                frame_bytes.reserve(64);

                for (size_t sym_idx = 0; phr_pos + (sym_idx + 2) * 32 <= chips.size() && frame_bytes.size() < 128; sym_idx += 2) {
                    uint32_t w_low = 0, w_high = 0;
                    for (int k = 0; k < 32; ++k) {
                        w_low |= (static_cast<uint32_t>(chips[phr_pos + sym_idx * 32 + k]) << k);
                        w_high |= (static_cast<uint32_t>(chips[phr_pos + (sym_idx + 1) * 32 + k]) << k);
                    }
                    int s_low_sc = 0, s_high_sc = 0;
                    uint8_t nib_low = correlate_chip_sequence(w_low, s_low_sc);
                    uint8_t nib_high = correlate_chip_sequence(w_high, s_high_sc);

                    uint8_t byte_val = (nib_high << 4) | nib_low;
                    frame_bytes.push_back(byte_val);
                }

                if (frame_bytes.size() >= 5) {
                    uint8_t phr_len = frame_bytes[0] & 0x7F;
                    if (phr_len >= 4 && phr_len + 1 <= frame_bytes.size()) {
                        std::vector<uint8_t> psdu(frame_bytes.begin() + 1, frame_bytes.begin() + 1 + phr_len);

                        auto pkt = std::make_shared<Packet>();
                        pkt->protocol = ProtocolType::ZIGBEE;
                        pkt->center_freq_hz = target_freq_hz;
                        pkt->rssi_dbm = rssi_dbm;
                        pkt->zigbee.zigbee_channel = zb_channel_num;
                        pkt->raw_data = psdu;

                        if (ZigbeeDissector::dissect(*pkt)) {
                            if (packet_callback) {
                                packet_callback(pkt);
                            }
                            c += phr_len * 64; // Advance past packet
                        }
                    }
                }
            }
        }
    }
}

} // namespace discan
