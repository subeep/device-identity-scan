#pragma once

#include "sdr/sdr_device.hpp"
#include "protocols/packet.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <random>

namespace discan {

// Packet injection callback for passing decoded/synthetic packets directly to the pipeline
using PacketInjectionCallback = std::function<void(const PacketPtr&)>;

class SimulatedDevice : public ISdrDevice {
public:
    SimulatedDevice();
    ~SimulatedDevice() override;

    SdrDeviceType get_type() const override { return SdrDeviceType::SIMULATED; }
    std::string get_name() const override { return "Synthetic RF & Protocol Simulator"; }
    std::string get_serial() const override { return "SIM-802-BLE-ZB-LORA"; }

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

    void set_packet_callback(PacketInjectionCallback pkt_cb) {
        packet_callback_ = pkt_cb;
    }

private:
    std::atomic<SdrState> state_{SdrState::READY};
    std::string status_msg_{"Simulator ready"};

    double current_freq_hz_ = 2437000000.0;
    double current_rate_sps_ = 20000000.0;
    double current_bw_hz_ = 20000000.0;
    double gain_db_ = 35.0;

    SdrRxCallback rx_callback_ = nullptr;
    PacketInjectionCallback packet_callback_ = nullptr;

    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;

    std::mt19937 rng_{42};

    void worker_loop();
    void generate_simulated_traffic();
    
    // Packet generators for realistic protocol simulation
    PacketPtr generate_wifi_beacon(int channel, const std::string& ssid, const std::string& bssid, const std::string& enc);
    PacketPtr generate_wifi_probe_req(const std::string& client_mac, const std::string& target_ssid);
    PacketPtr generate_ble_adv(int channel, const std::string& mac, const std::string& local_name, bool is_beacon, uint16_t major = 0, uint16_t minor = 0);
    PacketPtr generate_zigbee_beacon(int channel, uint16_t pan_id, const std::string& coord_mac);
    PacketPtr generate_zigbee_data(int channel, uint16_t pan_id, uint16_t src_short, uint16_t dst_short);
    PacketPtr generate_lora_join_req(const std::string& dev_eui, const std::string& join_eui);
    PacketPtr generate_lora_data_up(const std::string& dev_addr, uint16_t fcnt, int sf);
};

} // namespace discan
