#pragma once

#include "sdr/sdr_device.hpp"
#include <atomic>
#include <thread>
#include <mutex>

namespace discan {

class HackRfDevice : public ISdrDevice {
public:
    HackRfDevice();
    ~HackRfDevice() override;

    SdrDeviceType get_type() const override { return SdrDeviceType::HACKRF_ONE; }
    std::string get_name() const override { return "Great Scott Gadgets HackRF One"; }
    std::string get_serial() const override { return serial_number_; }

    bool initialize() override;
    bool start_rx(SdrRxCallback callback) override;
    bool stop_rx() override;
    void close() override;

    bool set_frequency(double freq_hz) override;
    bool set_sample_rate(double rate_sps) override;
    bool set_bandwidth(double bw_hz) override;
    bool set_gain(double gain_db) override;
    bool set_hackrf_gains(int lna_gain, int vga_gain, bool amp_enable) override;

    SdrState get_state() const override { return state_; }
    std::string get_status_message() const override { return status_msg_; }
    double get_actual_frequency() const override { return current_freq_hz_; }
    double get_actual_sample_rate() const override { return current_rate_sps_; }

    // HackRF RX callback hook
    void on_hackrf_rx(const int8_t* raw_bytes, size_t length);

private:
    std::atomic<SdrState> state_{SdrState::DISCONNECTED};
    std::string status_msg_{"Not initialized"};
    std::string serial_number_{"Unknown"};

    double current_freq_hz_ = 2437000000.0;
    double current_rate_sps_ = 20000000.0;
    double current_bw_hz_ = 20000000.0;
    int lna_gain_ = 32;
    int vga_gain_ = 30;
    bool amp_enable_ = false;

    void* lib_handle_ = nullptr;
    void* hackrf_dev_ = nullptr;
    SdrRxCallback rx_callback_ = nullptr;

    bool load_hackrf_library();
    void unload_hackrf_library();
};

} // namespace discan
