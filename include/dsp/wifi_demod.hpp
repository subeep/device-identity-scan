#pragma once

#include "common/types.hpp"
#include "protocols/packet.hpp"
#include <vector>
#include <complex>
#include <functional>
#include <cstdint>

namespace discan {

class WifiDemodulator {
public:
    explicit WifiDemodulator(double default_sample_rate = 20000000.0);
    ~WifiDemodulator() = default;

    void process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                            std::function<void(const PacketPtr&)> packet_callback);

private:
    double sample_rate_ = 20000000.0;

    // 11-chip Barker Code sequence (+1 -1 +1 +1 -1 +1 +1 +1 -1 -1 -1)
    static const int8_t barker_code[11];

    void demodulate_dsss_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                 int channel_num, double target_freq_hz,
                                 std::function<void(const PacketPtr&)>& packet_callback);
};

} // namespace discan
