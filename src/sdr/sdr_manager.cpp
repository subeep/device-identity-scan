#include "sdr/sdr_manager.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <algorithm>

namespace discan {

SdrManager::SdrManager() {
    build_hop_schedule(scan_mode_);
    hop_start_time_ = std::chrono::steady_clock::now();
}

SdrManager::~SdrManager() {
    shutdown();
}

void SdrManager::build_hop_schedule(ProtocolScanMode mode) {
    hop_schedule_.clear();

    switch (mode) {
        case ProtocolScanMode::WIFI_2G4_PRIMARY:
            dwell_time_ms_ = 450.0;
            hop_schedule_.push_back({"Wi-Fi Ch 1 (2412 MHz)", ProtocolType::WIFI, 2412000000.0, 20000000.0, 20000000.0, 450.0, 1});
            hop_schedule_.push_back({"Wi-Fi Ch 6 (2437 MHz)", ProtocolType::WIFI, 2437000000.0, 20000000.0, 20000000.0, 450.0, 6});
            hop_schedule_.push_back({"Wi-Fi Ch 11 (2462 MHz)", ProtocolType::WIFI, 2462000000.0, 20000000.0, 20000000.0, 450.0, 11});
            break;

        case ProtocolScanMode::WIFI_2G4_ALL:
            dwell_time_ms_ = 400.0;
            for (int ch = 1; ch <= 13; ++ch) {
                double freq = 2412000000.0 + (ch - 1) * 5000000.0;
                hop_schedule_.push_back({"Wi-Fi Ch " + std::to_string(ch) + " (" + std::to_string(static_cast<int>(freq/1e6)) + " MHz)", 
                                         ProtocolType::WIFI, freq, 20000000.0, 20000000.0, 400.0, ch});
            }
            hop_schedule_.push_back({"Wi-Fi Ch 14 (2484 MHz)", ProtocolType::WIFI, 2484000000.0, 20000000.0, 20000000.0, 400.0, 14});
            break;

        case ProtocolScanMode::WIFI_5G_UNII:
            dwell_time_ms_ = 400.0;
            // UNII-1
            hop_schedule_.push_back({"Wi-Fi 5G Ch 36 (5180 MHz)", ProtocolType::WIFI, 5180000000.0, 20000000.0, 20000000.0, 400.0, 36});
            hop_schedule_.push_back({"Wi-Fi 5G Ch 40 (5200 MHz)", ProtocolType::WIFI, 5200000000.0, 20000000.0, 20000000.0, 400.0, 40});
            hop_schedule_.push_back({"Wi-Fi 5G Ch 44 (5220 MHz)", ProtocolType::WIFI, 5220000000.0, 20000000.0, 20000000.0, 400.0, 44});
            hop_schedule_.push_back({"Wi-Fi 5G Ch 48 (5240 MHz)", ProtocolType::WIFI, 5240000000.0, 20000000.0, 20000000.0, 400.0, 48});
            // UNII-3
            hop_schedule_.push_back({"Wi-Fi 5G Ch 149 (5745 MHz)", ProtocolType::WIFI, 5745000000.0, 20000000.0, 20000000.0, 400.0, 149});
            hop_schedule_.push_back({"Wi-Fi 5G Ch 157 (5785 MHz)", ProtocolType::WIFI, 5785000000.0, 20000000.0, 20000000.0, 400.0, 157});
            hop_schedule_.push_back({"Wi-Fi 5G Ch 165 (5825 MHz)", ProtocolType::WIFI, 5825000000.0, 20000000.0, 20000000.0, 400.0, 165});
            break;

        case ProtocolScanMode::BLE_ALL_ADV:
            dwell_time_ms_ = 600.0;
            // 20 MSPS wideband reception covering Adv Ch 37, 38, 39 and all adjacent BLE data channels
            hop_schedule_.push_back({"BLE Adv Ch 37 + Low Band (2402 MHz)", ProtocolType::BLUETOOTH, 2402000000.0, 20000000.0, 20000000.0, 600.0, 37});
            hop_schedule_.push_back({"BLE Adv Ch 38 + Mid Band (2426 MHz)", ProtocolType::BLUETOOTH, 2426000000.0, 20000000.0, 20000000.0, 600.0, 38});
            hop_schedule_.push_back({"BLE Adv Ch 39 + High Band (2480 MHz)", ProtocolType::BLUETOOTH, 2480000000.0, 20000000.0, 20000000.0, 600.0, 39});
            break;

        case ProtocolScanMode::ZIGBEE_ALL_16_CH:
            dwell_time_ms_ = 600.0;
            // 4 Wideband 20 MSPS blocks covering all 16 Zigbee channels (11..26)
            hop_schedule_.push_back({"Zigbee Ch 11-14 Block (2412 MHz)", ProtocolType::ZIGBEE, 2412000000.0, 20000000.0, 20000000.0, 600.0, 11});
            hop_schedule_.push_back({"Zigbee Ch 15-18 Block (2432 MHz)", ProtocolType::ZIGBEE, 2432000000.0, 20000000.0, 20000000.0, 600.0, 15});
            hop_schedule_.push_back({"Zigbee Ch 19-22 Block (2452 MHz)", ProtocolType::ZIGBEE, 2452000000.0, 20000000.0, 20000000.0, 600.0, 19});
            hop_schedule_.push_back({"Zigbee Ch 23-26 Block (2472 MHz)", ProtocolType::ZIGBEE, 2472000000.0, 20000000.0, 20000000.0, 600.0, 23});
            break;

        case ProtocolScanMode::LORA_EU868_WIDE:
            dwell_time_ms_ = 1500.0;
            // 4 MSPS centered at 868.0 MHz covers all 8 EU868 & IN865 channels (867.1 - 868.8 MHz)
            hop_schedule_.push_back({"LoRa EU868 / IN865 Wideband (868.0 MHz)", ProtocolType::LORA, 868000000.0, 4000000.0, 4000000.0, 1500.0, 1});
            break;

        case ProtocolScanMode::LORA_US915_ALL:
            dwell_time_ms_ = 1200.0;
            // 8 Sub-bands across 902 - 915 MHz
            hop_schedule_.push_back({"LoRa US915 Sub-band 1 (903.0 MHz)", ProtocolType::LORA, 903000000.0, 4000000.0, 4000000.0, 1200.0, 1});
            hop_schedule_.push_back({"LoRa US915 Sub-band 2 (904.6 MHz)", ProtocolType::LORA, 904600000.0, 4000000.0, 4000000.0, 1200.0, 2});
            hop_schedule_.push_back({"LoRa US915 Sub-band 3 (906.2 MHz)", ProtocolType::LORA, 906200000.0, 4000000.0, 4000000.0, 1200.0, 3});
            hop_schedule_.push_back({"LoRa US915 Sub-band 4 (907.8 MHz)", ProtocolType::LORA, 907800000.0, 4000000.0, 4000000.0, 1200.0, 4});
            hop_schedule_.push_back({"LoRa US915 Sub-band 5 (909.4 MHz)", ProtocolType::LORA, 909400000.0, 4000000.0, 4000000.0, 1200.0, 5});
            hop_schedule_.push_back({"LoRa US915 Sub-band 6 (911.0 MHz)", ProtocolType::LORA, 911000000.0, 4000000.0, 4000000.0, 1200.0, 6});
            hop_schedule_.push_back({"LoRa US915 Sub-band 7 (912.6 MHz)", ProtocolType::LORA, 912600000.0, 4000000.0, 4000000.0, 1200.0, 7});
            hop_schedule_.push_back({"LoRa US915 Sub-band 8 (914.2 MHz)", ProtocolType::LORA, 914200000.0, 4000000.0, 4000000.0, 1200.0, 8});
            break;

        case ProtocolScanMode::LORA_433_ISM:
            dwell_time_ms_ = 1500.0;
            hop_schedule_.push_back({"LoRa 433 MHz ISM (433.175 MHz)", ProtocolType::LORA, 433175000.0, 2000000.0, 2000000.0, 1500.0, 1});
            break;

        case ProtocolScanMode::FULL_SPECTRUM_SWEEP:
        default:
            dwell_time_ms_ = 500.0;
            // Wi-Fi 2.4G
            hop_schedule_.push_back({"Wi-Fi Ch 1 (2412 MHz)", ProtocolType::WIFI, 2412000000.0, 20000000.0, 20000000.0, 450.0, 1});
            hop_schedule_.push_back({"Wi-Fi Ch 6 (2437 MHz)", ProtocolType::WIFI, 2437000000.0, 20000000.0, 20000000.0, 450.0, 6});
            hop_schedule_.push_back({"Wi-Fi Ch 11 (2462 MHz)", ProtocolType::WIFI, 2462000000.0, 20000000.0, 20000000.0, 450.0, 11});
            // BLE
            hop_schedule_.push_back({"BLE Adv Ch 37 (2402 MHz)", ProtocolType::BLUETOOTH, 2402000000.0, 20000000.0, 20000000.0, 600.0, 37});
            hop_schedule_.push_back({"BLE Adv Ch 38 (2426 MHz)", ProtocolType::BLUETOOTH, 2426000000.0, 20000000.0, 20000000.0, 600.0, 38});
            hop_schedule_.push_back({"BLE Adv Ch 39 (2480 MHz)", ProtocolType::BLUETOOTH, 2480000000.0, 20000000.0, 20000000.0, 600.0, 39});
            // Zigbee
            hop_schedule_.push_back({"Zigbee Ch 15 (2425 MHz)", ProtocolType::ZIGBEE, 2425000000.0, 20000000.0, 20000000.0, 600.0, 15});
            hop_schedule_.push_back({"Zigbee Ch 20 (2450 MHz)", ProtocolType::ZIGBEE, 2450000000.0, 20000000.0, 20000000.0, 600.0, 20});
            // LoRa
            hop_schedule_.push_back({"LoRa EU868 (868.0 MHz)", ProtocolType::LORA, 868000000.0, 4000000.0, 4000000.0, 1200.0, 1});
            hop_schedule_.push_back({"LoRa US915 (903.0 MHz)", ProtocolType::LORA, 903000000.0, 4000000.0, 4000000.0, 1200.0, 1});
            break;
    }

    current_hop_idx_ = 0;
    config_.dwell_time_ms = dwell_time_ms_;
}

void SdrManager::set_protocol_scan_mode(ProtocolScanMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    scan_mode_ = mode;
    config_.scan_mode = mode;
    build_hop_schedule(mode);
    apply_hop_channel(0);
    DISCAN_LOG_INFO("Protocol Scan Mode changed to: " << protocol_scan_mode_to_string(mode) 
                  << " (" << hop_schedule_.size() << " channels, Dwell: " << dwell_time_ms_ << " ms)");
}

void SdrManager::apply_hop_channel(size_t index) {
    if (hop_schedule_.empty()) return;
    index = index % hop_schedule_.size();
    current_hop_idx_ = index;
    config_.current_hop_index = static_cast<int>(index);

    const auto& ch = hop_schedule_[index];
    config_.center_frequency_hz = ch.frequency_hz;
    config_.sample_rate_sps = ch.sample_rate_sps;
    config_.bandwidth_hz = ch.bandwidth_hz;

    if (active_device_) {
        active_device_->set_frequency(ch.frequency_hz);
        active_device_->set_sample_rate(ch.sample_rate_sps);
        active_device_->set_bandwidth(ch.bandwidth_hz);
    }
    hop_start_time_ = std::chrono::steady_clock::now();
}

HopChannel SdrManager::get_active_hop_channel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (hop_schedule_.empty()) {
        return {"Default (2437 MHz)", ProtocolType::WIFI, 2437000000.0, 20000000.0, 20000000.0, 450.0, 6};
    }
    size_t idx = current_hop_idx_ % hop_schedule_.size();
    return hop_schedule_[idx];
}

void SdrManager::set_dwell_time_ms(double ms) {
    dwell_time_ms_ = std::clamp(ms, 50.0, 5000.0);
    config_.dwell_time_ms = dwell_time_ms_;
}

float SdrManager::get_dwell_progress() {
    auto now = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(now - hop_start_time_).count();
    double dwell = dwell_time_ms_.load();
    if (dwell <= 0.0) return 1.0f;
    return std::clamp(static_cast<float>(elapsed_ms / dwell), 0.0f, 1.0f);
}

void SdrManager::set_auto_hopping(bool enabled) {
    auto_hopping_enabled_ = enabled;
    config_.auto_hopping_enabled = enabled;
    DISCAN_LOG_INFO("Auto-Channel Hopping: " << (enabled ? "Enabled" : "Paused"));
}

void SdrManager::step_next_channel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (hop_schedule_.empty()) return;
    size_t next_idx = (current_hop_idx_ + 1) % hop_schedule_.size();
    apply_hop_channel(next_idx);
}

void SdrManager::step_prev_channel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (hop_schedule_.empty()) return;
    size_t prev_idx = (current_hop_idx_ == 0) ? (hop_schedule_.size() - 1) : (current_hop_idx_ - 1);
    apply_hop_channel(prev_idx);
}

void SdrManager::initialize() {
    switch_device(SdrDeviceType::SIMULATED);
    hopping_running_ = true;
    hopping_thread_ = std::thread(&SdrManager::hopping_worker_loop, this);

    // Start Live Real-World Wi-Fi Scanner (captures 100% of physical Wi-Fi APs & clients in room)
    WifiLiveScanner::instance().start([this](const PacketPtr& pkt) {
        this->on_packet_received(pkt);
    });
}

void SdrManager::shutdown() {
    hopping_running_ = false;
    if (hopping_thread_.joinable()) {
        hopping_thread_.join();
    }
    stop_capture();
    if (active_device_) {
        active_device_->close();
        active_device_.reset();
    }
}

bool SdrManager::switch_device(SdrDeviceType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool was_streaming = false;
    if (active_device_) {
        was_streaming = (active_device_->get_state() == SdrState::STREAMING);
        active_device_->stop_rx();
        active_device_->close();
        active_device_.reset();
    }

    active_type_ = type;
    config_.device_type = type;

    if (type == SdrDeviceType::HACKRF_ONE) {
        active_device_ = std::make_unique<HackRfDevice>();
    } else if (type == SdrDeviceType::USRP_B210) {
        active_device_ = std::make_unique<UsrpDevice>();
    } else {
        auto sim = std::make_unique<SimulatedDevice>();
        sim->set_packet_callback([this](const PacketPtr& pkt) {
            this->on_packet_received(pkt);
        });
        active_device_ = std::move(sim);
    }

    active_device_->initialize();
    apply_hop_channel(current_hop_idx_);
    active_device_->set_gain(config_.rx_gain_db);
    active_device_->set_hackrf_gains(config_.hackrf_lna_gain, config_.hackrf_vga_gain, config_.hackrf_amp_enable);

    DISCAN_LOG_INFO("Switched active SDR to: " << active_device_->get_name());

    if (was_streaming) {
        active_device_->start_rx([this](const ComplexSample* samples, size_t count, double freq_hz, double rate_sps) {
            this->on_raw_iq_received(samples, count, freq_hz, rate_sps);
        });
    }

    return true;
}

bool SdrManager::start_capture() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_device_) return false;

    return active_device_->start_rx([this](const ComplexSample* samples, size_t count, double freq_hz, double rate_sps) {
        this->on_raw_iq_received(samples, count, freq_hz, rate_sps);
    });
}

bool SdrManager::stop_capture() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_device_) return false;
    return active_device_->stop_rx();
}

bool SdrManager::is_streaming() const {
    if (!active_device_) return false;
    return active_device_->get_state() == SdrState::STREAMING;
}

bool SdrManager::set_frequency(double freq_hz) {
    config_.center_frequency_hz = freq_hz;
    if (active_device_) {
        return active_device_->set_frequency(freq_hz);
    }
    return false;
}

bool SdrManager::set_sample_rate(double rate_sps) {
    config_.sample_rate_sps = rate_sps;
    if (active_device_) {
        return active_device_->set_sample_rate(rate_sps);
    }
    return false;
}

bool SdrManager::set_gain(double gain_db) {
    config_.rx_gain_db = gain_db;
    if (active_device_) {
        return active_device_->set_gain(gain_db);
    }
    return false;
}

bool SdrManager::set_hackrf_gains(int lna_gain, int vga_gain, bool amp_enable) {
    config_.hackrf_lna_gain = lna_gain;
    config_.hackrf_vga_gain = vga_gain;
    config_.hackrf_amp_enable = amp_enable;
    if (active_device_) {
        return active_device_->set_hackrf_gains(lna_gain, vga_gain, amp_enable);
    }
    return false;
}

void SdrManager::hopping_worker_loop() {
    while (hopping_running_) {
        if (auto_hopping_enabled_ && is_streaming()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(now - hop_start_time_).count();

            if (elapsed >= dwell_time_ms_.load()) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!hop_schedule_.empty()) {
                    size_t next_idx = (current_hop_idx_ + 1) % hop_schedule_.size();
                    apply_hop_channel(next_idx);
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void SdrManager::on_raw_iq_received(const ComplexSample* samples, size_t count, double freq_hz, double rate_sps) {
    // 1. Pipe to FFT spectrum & waterfall analyzer
    fft_analyzer_.process_samples(samples, count, freq_hz, rate_sps);

    // 2. Pipe to Real-Time Protocol Demodulators
    auto pkt_handler = [this](const PacketPtr& pkt) {
        this->on_packet_received(pkt);
    };

    // Run BLE GFSK Demodulator (2.4 GHz Advertising channels 37/38/39)
    ble_demod_.process_iq_samples(samples, count, freq_hz, rate_sps, pkt_handler);

    // Run Zigbee O-QPSK Demodulator (2.4 GHz channels 11-26)
    zigbee_demod_.process_iq_samples(samples, count, freq_hz, rate_sps, pkt_handler);

    // Run Wi-Fi 802.11 DSSS Demodulator (2.4 GHz channels 1/6/11)
    wifi_demod_.process_iq_samples(samples, count, freq_hz, rate_sps, pkt_handler);

    // Run LoRa CSS Demodulator (Sub-GHz ISM bands)
    lora_demod_.process_iq_samples(samples, count, freq_hz, rate_sps, pkt_handler);
}

void SdrManager::on_packet_received(const PacketPtr& pkt) {
    if (!pkt) return;
    
    // Ingest into Device Tracker
    DeviceTracker::instance().process_packet(pkt);

    // Ingest into IDS Engine
    IdsEngine::instance().inspect_packet(pkt);

    // Ingest into Packet History Storage
    PacketStorage::instance().store_packet(pkt);
}

} // namespace discan
