#pragma once

#include "common/types.hpp"
#include "sdr/sdr_device.hpp"
#include "sdr/hackrf_device.hpp"
#include "sdr/usrp_device.hpp"
#include "sdr/simulated_device.hpp"
#include "dsp/fft_analyzer.hpp"
#include "dsp/ble_demod.hpp"
#include "dsp/zigbee_demod.hpp"
#include "dsp/lora_demod.hpp"
#include "dsp/wifi_demod.hpp"
#include "core/device_tracker.hpp"
#include "core/ids_engine.hpp"
#include "core/packet_storage.hpp"
#include "core/wifi_live_scanner.hpp"
#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

namespace discan {

class SdrManager {
public:
    static SdrManager& instance() {
        static SdrManager inst;
        return inst;
    }

    void initialize();
    void shutdown();

    // Device selection
    bool switch_device(SdrDeviceType type);
    SdrDeviceType get_active_device_type() const { return active_type_; }
    ISdrDevice* get_active_device() const { return active_device_.get(); }
    
    // Streaming control
    bool start_capture();
    bool stop_capture();
    bool is_streaming() const;

    // Tuning & RF configuration
    bool set_frequency(double freq_hz);
    bool set_sample_rate(double rate_sps);
    bool set_gain(double gain_db);
    bool set_hackrf_gains(int lna_gain, int vga_gain, bool amp_enable);

    // Protocol Scan Modes & Verified Hopping Schedules
    void set_protocol_scan_mode(ProtocolScanMode mode);
    ProtocolScanMode get_protocol_scan_mode() const { return scan_mode_; }
    
    const std::vector<HopChannel>& get_hop_schedule() const { return hop_schedule_; }
    HopChannel get_active_hop_channel();
    size_t get_current_hop_index() const { return current_hop_idx_; }

    void set_auto_hopping(bool enabled);
    bool is_auto_hopping() const { return auto_hopping_enabled_; }

    void set_dwell_time_ms(double ms);
    double get_dwell_time_ms() const { return dwell_time_ms_; }
    float get_dwell_progress(); // 0.0 to 1.0

    void step_next_channel();
    void step_prev_channel();

    // DSP / FFT access
    FftAnalyzer& get_fft_analyzer() { return fft_analyzer_; }

    // Direct packet injection hook
    void on_packet_received(const PacketPtr& pkt);

    SdrConfig& get_config() { return config_; }

private:
    SdrManager();
    ~SdrManager();

    std::mutex mutex_;
    SdrConfig config_;
    SdrDeviceType active_type_ = SdrDeviceType::SIMULATED;
    std::unique_ptr<ISdrDevice> active_device_;

    FftAnalyzer fft_analyzer_{512, 120};
    BleDemodulator ble_demod_;
    ZigbeeDemodulator zigbee_demod_;
    LoraDemodulator lora_demod_;
    WifiDemodulator wifi_demod_;

    ProtocolScanMode scan_mode_ = ProtocolScanMode::WIFI_2G4_PRIMARY;
    std::vector<HopChannel> hop_schedule_;
    std::atomic<size_t> current_hop_idx_{0};
    std::atomic<bool> auto_hopping_enabled_{true};
    std::atomic<double> dwell_time_ms_{450.0};
    std::chrono::steady_clock::time_point hop_start_time_;

    std::thread hopping_thread_;
    std::atomic<bool> hopping_running_{false};

    void build_hop_schedule(ProtocolScanMode mode);
    void apply_hop_channel(size_t index);
    void hopping_worker_loop();
    void on_raw_iq_received(const ComplexSample* samples, size_t count, double freq_hz, double rate_sps);
};

} // namespace discan
