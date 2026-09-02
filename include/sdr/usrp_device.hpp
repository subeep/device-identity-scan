#pragma once

#include "sdr/sdr_device.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>

namespace uhd {
namespace usrp {
    class multi_usrp;
}
}

namespace discan {

class UsrpDevice : public ISdrDevice {
public:
    UsrpDevice();
    ~UsrpDevice() override;

    SdrDeviceType get_type() const override { return SdrDeviceType::USRP_B210; }
    std::string get_name() const override { return "Ettus Research USRP B210"; }
    std::string get_serial() const override { return serial_number_; }

    bool initialize() override;
    bool start_rx(SdrRxCallback callback) override;
    bool stop_rx() override;
    void close() override;

    bool set_frequency(double freq_hz) override;
    bool set_sample_rate(double rate_sps) override;
    bool set_bandwidth(double bw_hz) override;
    bool set_gain(double gain_db) override;

    SdrState get_state() const override { return state_; }
    std::string get_status_message() const override { return status_msg_; }
    double get_actual_frequency() const override { return current_freq_hz_; }
    double get_actual_sample_rate() const override { return current_rate_sps_; }

private:
    std::atomic<SdrState> state_{SdrState::DISCONNECTED};
    std::string status_msg_{"Not initialized"};
    std::string serial_number_{"Unknown"};

    double current_freq_hz_ = 2437000000.0;
    double current_rate_sps_ = 20000000.0;
    double current_bw_hz_ = 20000000.0;
    double current_gain_db_ = 38.0;

    std::shared_ptr<uhd::usrp::multi_usrp> usrp_;
    SdrRxCallback rx_callback_ = nullptr;
    
    std::atomic<bool> rx_running_{false};
    std::thread rx_worker_thread_;

    void rx_worker_loop();
};

} // namespace discan
