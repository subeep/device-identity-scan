#pragma once

#include "common/types.hpp"
#include "protocols/packet.hpp"
#include <vector>
#include <complex>
#include <functional>
#include <cstdint>

namespace discan {

class ZigbeeDemodulator {
public:
    explicit ZigbeeDemodulator(double default_sample_rate = 20000000.0);
    ~ZigbeeDemodulator() = default;

    // Process IQ samples from SDR
    void process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                            std::function<void(const PacketPtr&)> packet_callback);

private:
    double sample_rate_ = 20000000.0;

    void demodulate_zigbee_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                   int zb_channel_num, double target_freq_hz,
                                   std::function<void(const PacketPtr&)>& packet_callback);

    // 16 Standard IEEE 802.15.4 32-chip pseudo-random sequences
    static const uint32_t chip_sequences_32[16];
    static uint8_t correlate_chip_sequence(uint32_t chip_word, int& out_match_score);
    static bool check_crc16(const uint8_t* data, size_t len);
};

} // namespace discan
