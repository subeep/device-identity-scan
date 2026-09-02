#pragma once

#include "common/types.hpp"
#include <functional>
#include <string>
#include <memory>

namespace discan {

// Callback function type for raw IQ streaming: (samples_ptr, count, center_freq, sample_rate)
using SdrRxCallback = std::function<void(const ComplexSample*, size_t, double, double)>;

class ISdrDevice {
public:
    virtual ~ISdrDevice() = default;

    virtual SdrDeviceType get_type() const = 0;
    virtual std::string get_name() const = 0;
    virtual std::string get_serial() const = 0;

    virtual bool initialize() = 0;
    virtual bool start_rx(SdrRxCallback callback) = 0;
    virtual bool stop_rx() = 0;
    virtual void close() = 0;

    virtual bool set_frequency(double freq_hz) = 0;
    virtual bool set_sample_rate(double rate_sps) = 0;
    virtual bool set_bandwidth(double bw_hz) = 0;
    virtual bool set_gain(double gain_db) = 0;
    virtual bool set_hackrf_gains(int lna_gain, int vga_gain, bool amp_enable) {
        (void)lna_gain; (void)vga_gain; (void)amp_enable;
        return false;
    }

    virtual SdrState get_state() const = 0;
    virtual std::string get_status_message() const = 0;
    virtual double get_actual_frequency() const = 0;
    virtual double get_actual_sample_rate() const = 0;
};

using SdrDevicePtr = std::shared_ptr<ISdrDevice>;

} // namespace discan
