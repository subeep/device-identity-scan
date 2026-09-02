#include "sdr/usrp_device.hpp"
#include "common/logger.hpp"
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/thread.hpp>
#include <uhd/types/tune_request.hpp>
#include <vector>

namespace discan {

UsrpDevice::UsrpDevice() {
    status_msg_ = "USRP B210 driver initialized";
}

UsrpDevice::~UsrpDevice() {
    close();
}

bool UsrpDevice::initialize() {
    try {
        state_ = SdrState::INITIALIZING;
        status_msg_ = "Scanning for USRP B210 devices...";
        
        uhd::device_addrs_t addrs = uhd::device::find(uhd::device_addr_t("type=b200"));
        if (addrs.empty()) {
            addrs = uhd::device::find(uhd::device_addr_t(""));
        }

        if (addrs.empty()) {
            status_msg_ = "No USRP B210 hardware device found";
            state_ = SdrState::DISCONNECTED;
            DISCAN_LOG_WARN("No USRP hardware detected via UHD");
            return false;
        }

        std::string args = addrs[0].to_string();
        DISCAN_LOG_INFO("Opening USRP device: " << args);
        usrp_ = uhd::usrp::multi_usrp::make(args);

        // Configure master clock rate for B210 (e.g. 20 MHz / 56 MHz)
        usrp_->set_master_clock_rate(current_rate_sps_);
        usrp_->set_rx_rate(current_rate_sps_);
        usrp_->set_rx_freq(uhd::tune_request_t(current_freq_hz_));
        usrp_->set_rx_gain(current_gain_db_);
        usrp_->set_rx_bandwidth(current_bw_hz_);
        usrp_->set_rx_subdev_spec(uhd::usrp::subdev_spec_t("A:A"));

        serial_number_ = usrp_->get_usrp_rx_info()["mboard_serial"];

        state_ = SdrState::READY;
        status_msg_ = "USRP B210 connected (S/N: " + serial_number_ + ")";
        DISCAN_LOG_INFO("USRP B210 initialized successfully, S/N: " << serial_number_);
        return true;
    } catch (const std::exception& ex) {
        status_msg_ = std::string("UHD Error: ") + ex.what();
        state_ = SdrState::ERROR_STATE;
        DISCAN_LOG_ERROR("UHD Exception during initialize: " << ex.what());
        return false;
    }
}

bool UsrpDevice::start_rx(SdrRxCallback callback) {
    if (state_ != SdrState::READY || !usrp_) {
        if (!initialize()) return false;
    }

    rx_callback_ = callback;
    rx_running_ = true;
    state_ = SdrState::STREAMING;
    status_msg_ = "USRP B210 streaming @ " + std::to_string(current_freq_hz_ / 1e6) + " MHz";

    rx_worker_thread_ = std::thread(&UsrpDevice::rx_worker_loop, this);
    DISCAN_LOG_INFO("USRP B210 worker thread launched");
    return true;
}

bool UsrpDevice::stop_rx() {
    if (rx_running_) {
        rx_running_ = false;
        if (rx_worker_thread_.joinable()) {
            rx_worker_thread_.join();
        }
        state_ = SdrState::READY;
        status_msg_ = "USRP B210 stopped";
        DISCAN_LOG_INFO("USRP B210 RX stopped");
        return true;
    }
    return false;
}

void UsrpDevice::close() {
    stop_rx();
    usrp_.reset();
    state_ = SdrState::DISCONNECTED;
    status_msg_ = "USRP closed";
}

bool UsrpDevice::set_frequency(double freq_hz) {
    current_freq_hz_ = freq_hz;
    if (usrp_) {
        try {
            usrp_->set_rx_freq(uhd::tune_request_t(freq_hz));
            return true;
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool UsrpDevice::set_sample_rate(double rate_sps) {
    current_rate_sps_ = rate_sps;
    if (usrp_) {
        try {
            usrp_->set_rx_rate(rate_sps);
            return true;
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool UsrpDevice::set_bandwidth(double bw_hz) {
    current_bw_hz_ = bw_hz;
    if (usrp_) {
        try {
            usrp_->set_rx_bandwidth(bw_hz);
            return true;
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool UsrpDevice::set_gain(double gain_db) {
    current_gain_db_ = gain_db;
    if (usrp_) {
        try {
            usrp_->set_rx_gain(gain_db);
            return true;
        } catch (...) {
            return false;
        }
    }
    return true;
}

void UsrpDevice::rx_worker_loop() {
    uhd::set_thread_priority_safe(1.0, true);

    try {
        uhd::stream_args_t stream_args("fc32", "sc16");
        uhd::rx_streamer::sptr rx_stream = usrp_->get_rx_stream(stream_args);

        const size_t max_samps_per_packet = rx_stream->get_max_num_samps();
        std::vector<ComplexSample> buff(max_samps_per_packet);

        uhd::rx_metadata_t md;
        uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
        stream_cmd.stream_now = true;
        rx_stream->issue_stream_cmd(stream_cmd);

        while (rx_running_) {
            size_t num_rx_samps = rx_stream->recv(&buff.front(), buff.size(), md, 3.0, false);

            if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_NONE && num_rx_samps > 0) {
                if (rx_callback_) {
                    rx_callback_(buff.data(), num_rx_samps, current_freq_hz_, current_rate_sps_);
                }
            } else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
                DISCAN_LOG_WARN("UHD Buffer Overflow (O)");
            }
        }

        stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
        rx_stream->issue_stream_cmd(stream_cmd);
    } catch (const std::exception& ex) {
        DISCAN_LOG_ERROR("Exception in USRP RX worker: " << ex.what());
    }
}

} // namespace discan
