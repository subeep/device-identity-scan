#pragma once

#include "common/types.hpp"
#include "protocols/packet.hpp"
#include <vector>
#include <complex>
#include <functional>
#include <cstdint>

namespace discan {

class LoraDemodulator {
public:
    explicit LoraDemodulator(double default_sample_rate = 2000000.0);
    ~LoraDemodulator() = default;

    void process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                            std::function<void(const PacketPtr&)> packet_callback);

private:
    double sample_rate_ = 2000000.0;
    int spreading_factor_ = 7;
    double bandwidth_hz_ = 125000.0;

    void demodulate_lora_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                 double target_freq_hz, int sf, double bw,
                                 std::function<void(const PacketPtr&)>& packet_callback);
};

} // namespace discan
