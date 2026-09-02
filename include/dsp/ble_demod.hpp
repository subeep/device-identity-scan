#pragma once

#include "common/types.hpp"
#include "protocols/packet.hpp"
#include <vector>
#include <complex>
#include <functional>
#include <cstdint>

namespace discan {

class BleDemodulator {
public:
    explicit BleDemodulator(double default_sample_rate = 20000000.0);
    ~BleDemodulator() = default;

    // Process a block of IQ samples from SDR
    void process_iq_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                            std::function<void(const PacketPtr&)> packet_callback);

    void set_sensitivity(float threshold_snr) { snr_threshold_ = threshold_snr; }

private:
    double sample_rate_ = 20000000.0;
    float snr_threshold_ = 5.0f;

    // Fast DDC + FM discriminator + bit recovery for a specific BLE channel
    void demodulate_ble_channel(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps,
                                int ble_channel_num, double target_freq_hz,
                                std::function<void(const PacketPtr&)>& packet_callback);

    // De-whitener for BLE LFSR
    static void dewhiten(uint8_t* data, size_t len, uint8_t channel);

    // CRC24 computation
    static bool check_crc24(const uint8_t* data, size_t len);
};

} // namespace discan
