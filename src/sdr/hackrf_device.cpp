#include "sdr/hackrf_device.hpp"
#include "common/logger.hpp"
#include <dlfcn.h>
#include <vector>

namespace discan {

// HackRF C Types
typedef struct hackrf_device hackrf_device;
typedef struct {
    hackrf_device* device;
    uint8_t* buffer;
    int buffer_length;
    int valid_length;
    void* rx_ctx;
    void* tx_ctx;
} hackrf_transfer;

typedef int (*hackrf_sample_block_cb_fn)(hackrf_transfer* transfer);

// Function Pointer types
typedef int (*pfn_hackrf_init)();
typedef int (*pfn_hackrf_exit)();
typedef int (*pfn_hackrf_open)(hackrf_device** device);
typedef int (*pfn_hackrf_close)(hackrf_device* device);
typedef int (*pfn_hackrf_start_rx)(hackrf_device* device, hackrf_sample_block_cb_fn callback, void* rx_ctx);
typedef int (*pfn_hackrf_stop_rx)(hackrf_device* device);
typedef int (*pfn_hackrf_set_freq)(hackrf_device* device, uint64_t freq_hz);
typedef int (*pfn_hackrf_set_sample_rate)(hackrf_device* device, double freq_hz);
typedef int (*pfn_hackrf_set_vga_gain)(hackrf_device* device, uint32_t value);
typedef int (*pfn_hackrf_set_lna_gain)(hackrf_device* device, uint32_t value);
typedef int (*pfn_hackrf_set_amp_enable)(hackrf_device* device, uint8_t value);
typedef int (*pfn_hackrf_set_baseband_filter_bandwidth)(hackrf_device* device, uint32_t bandwidth_hz);

static pfn_hackrf_init fn_hackrf_init = nullptr;
static pfn_hackrf_exit fn_hackrf_exit = nullptr;
static pfn_hackrf_open fn_hackrf_open = nullptr;
static pfn_hackrf_close fn_hackrf_close = nullptr;
static pfn_hackrf_start_rx fn_hackrf_start_rx = nullptr;
static pfn_hackrf_stop_rx fn_hackrf_stop_rx = nullptr;
static pfn_hackrf_set_freq fn_hackrf_set_freq = nullptr;
static pfn_hackrf_set_sample_rate fn_hackrf_set_sample_rate = nullptr;
static pfn_hackrf_set_vga_gain fn_hackrf_set_vga_gain = nullptr;
static pfn_hackrf_set_lna_gain fn_hackrf_set_lna_gain = nullptr;
static pfn_hackrf_set_amp_enable fn_hackrf_set_amp_enable = nullptr;
static pfn_hackrf_set_baseband_filter_bandwidth fn_hackrf_set_baseband_filter_bandwidth = nullptr;

static int global_hackrf_rx_callback(hackrf_transfer* transfer) {
    if (!transfer || !transfer->rx_ctx) return -1;
    auto* dev = static_cast<HackRfDevice*>(transfer->rx_ctx);
    dev->on_hackrf_rx(reinterpret_cast<const int8_t*>(transfer->buffer), transfer->valid_length);
    return 0;
}

HackRfDevice::HackRfDevice() {
    status_msg_ = "HackRF driver initialized";
}

HackRfDevice::~HackRfDevice() {
    close();
}

bool HackRfDevice::load_hackrf_library() {
    if (lib_handle_) return true;

    const char* lib_paths[] = {
        "libhackrf.so.0",
        "/usr/lib/x86_64-linux-gnu/libhackrf.so.0",
        "/usr/local/lib/libhackrf.so",
        "libhackrf.so",
        nullptr
    };

    for (int i = 0; lib_paths[i] != nullptr; ++i) {
        lib_handle_ = dlopen(lib_paths[i], RTLD_LAZY);
        if (lib_handle_) {
            DISCAN_LOG_INFO("Loaded libhackrf from: " << lib_paths[i]);
            break;
        }
    }

    if (!lib_handle_) {
        status_msg_ = "libhackrf not found on system";
        DISCAN_LOG_WARN("Could not dynamic load libhackrf.so.0");
        return false;
    }

    fn_hackrf_init = (pfn_hackrf_init)dlsym(lib_handle_, "hackrf_init");
    fn_hackrf_exit = (pfn_hackrf_exit)dlsym(lib_handle_, "hackrf_exit");
    fn_hackrf_open = (pfn_hackrf_open)dlsym(lib_handle_, "hackrf_open");
    fn_hackrf_close = (pfn_hackrf_close)dlsym(lib_handle_, "hackrf_close");
    fn_hackrf_start_rx = (pfn_hackrf_start_rx)dlsym(lib_handle_, "hackrf_start_rx");
    fn_hackrf_stop_rx = (pfn_hackrf_stop_rx)dlsym(lib_handle_, "hackrf_stop_rx");
    fn_hackrf_set_freq = (pfn_hackrf_set_freq)dlsym(lib_handle_, "hackrf_set_freq");
    fn_hackrf_set_sample_rate = (pfn_hackrf_set_sample_rate)dlsym(lib_handle_, "hackrf_set_sample_rate");
    fn_hackrf_set_vga_gain = (pfn_hackrf_set_vga_gain)dlsym(lib_handle_, "hackrf_set_vga_gain");
    fn_hackrf_set_lna_gain = (pfn_hackrf_set_lna_gain)dlsym(lib_handle_, "hackrf_set_lna_gain");
    fn_hackrf_set_amp_enable = (pfn_hackrf_set_amp_enable)dlsym(lib_handle_, "hackrf_set_amp_enable");
    fn_hackrf_set_baseband_filter_bandwidth = (pfn_hackrf_set_baseband_filter_bandwidth)dlsym(lib_handle_, "hackrf_set_baseband_filter_bandwidth");

    if (!fn_hackrf_init || !fn_hackrf_open || !fn_hackrf_start_rx) {
        status_msg_ = "Failed to resolve required libhackrf symbols";
        unload_hackrf_library();
        return false;
    }

    return true;
}

void HackRfDevice::unload_hackrf_library() {
    if (lib_handle_) {
        dlclose(lib_handle_);
        lib_handle_ = nullptr;
    }
}

bool HackRfDevice::initialize() {
    if (!load_hackrf_library()) {
        state_ = SdrState::ERROR_STATE;
        return false;
    }

    int ret = fn_hackrf_init();
    if (ret != 0) {
        status_msg_ = "hackrf_init() failed (code " + std::to_string(ret) + ")";
        state_ = SdrState::ERROR_STATE;
        return false;
    }

    hackrf_device* dev = nullptr;
    ret = fn_hackrf_open(&dev);
    if (ret != 0 || !dev) {
        status_msg_ = "No HackRF One hardware device detected via USB";
        DISCAN_LOG_WARN("HackRF One not found on USB: error " << ret);
        state_ = SdrState::DISCONNECTED;
        return false;
    }

    hackrf_dev_ = dev;
    state_ = SdrState::READY;
    status_msg_ = "HackRF One connected and ready";
    DISCAN_LOG_INFO("HackRF One opened successfully");

    set_frequency(current_freq_hz_);
    set_sample_rate(current_rate_sps_);
    set_hackrf_gains(lna_gain_, vga_gain_, amp_enable_);

    return true;
}

bool HackRfDevice::start_rx(SdrRxCallback callback) {
    if (state_ != SdrState::READY || !hackrf_dev_) {
        if (!initialize()) return false;
    }

    rx_callback_ = callback;
    int ret = fn_hackrf_start_rx(static_cast<hackrf_device*>(hackrf_dev_), global_hackrf_rx_callback, this);
    if (ret != 0) {
        status_msg_ = "hackrf_start_rx failed (" + std::to_string(ret) + ")";
        state_ = SdrState::ERROR_STATE;
        return false;
    }

    state_ = SdrState::STREAMING;
    status_msg_ = "HackRF One streaming @ " + std::to_string(current_freq_hz_ / 1e6) + " MHz";
    DISCAN_LOG_INFO("HackRF One RX stream started");
    return true;
}

bool HackRfDevice::stop_rx() {
    if (state_ == SdrState::STREAMING && hackrf_dev_) {
        fn_hackrf_stop_rx(static_cast<hackrf_device*>(hackrf_dev_));
        state_ = SdrState::READY;
        status_msg_ = "HackRF One stopped";
        DISCAN_LOG_INFO("HackRF One RX stream stopped");
        return true;
    }
    return false;
}

void HackRfDevice::close() {
    stop_rx();
    if (hackrf_dev_ && fn_hackrf_close) {
        fn_hackrf_close(static_cast<hackrf_device*>(hackrf_dev_));
        hackrf_dev_ = nullptr;
    }
    if (fn_hackrf_exit) {
        fn_hackrf_exit();
    }
    unload_hackrf_library();
    state_ = SdrState::DISCONNECTED;
    status_msg_ = "HackRF closed";
}

bool HackRfDevice::set_frequency(double freq_hz) {
    current_freq_hz_ = freq_hz;
    if (hackrf_dev_ && fn_hackrf_set_freq) {
        return fn_hackrf_set_freq(static_cast<hackrf_device*>(hackrf_dev_), static_cast<uint64_t>(freq_hz)) == 0;
    }
    return true;
}

bool HackRfDevice::set_sample_rate(double rate_sps) {
    current_rate_sps_ = rate_sps;
    if (hackrf_dev_ && fn_hackrf_set_sample_rate) {
        return fn_hackrf_set_sample_rate(static_cast<hackrf_device*>(hackrf_dev_), rate_sps) == 0;
    }
    return true;
}

bool HackRfDevice::set_bandwidth(double bw_hz) {
    current_bw_hz_ = bw_hz;
    if (hackrf_dev_ && fn_hackrf_set_baseband_filter_bandwidth) {
        return fn_hackrf_set_baseband_filter_bandwidth(static_cast<hackrf_device*>(hackrf_dev_), static_cast<uint32_t>(bw_hz)) == 0;
    }
    return true;
}

bool HackRfDevice::set_gain(double gain_db) {
    // Map general gain to LNA and VGA
    int lna = static_cast<int>(gain_db * 0.4);
    int vga = static_cast<int>(gain_db * 0.6);
    return set_hackrf_gains(lna, vga, amp_enable_);
}

bool HackRfDevice::set_hackrf_gains(int lna_gain, int vga_gain, bool amp_enable) {
    lna_gain_ = (lna_gain / 8) * 8; // Steps of 8 (0-40)
    vga_gain_ = (vga_gain / 2) * 2; // Steps of 2 (0-62)
    amp_enable_ = amp_enable;

    if (hackrf_dev_) {
        if (fn_hackrf_set_lna_gain) fn_hackrf_set_lna_gain(static_cast<hackrf_device*>(hackrf_dev_), lna_gain_);
        if (fn_hackrf_set_vga_gain) fn_hackrf_set_vga_gain(static_cast<hackrf_device*>(hackrf_dev_), vga_gain_);
        if (fn_hackrf_set_amp_enable) fn_hackrf_set_amp_enable(static_cast<hackrf_device*>(hackrf_dev_), amp_enable_ ? 1 : 0);
    }
    return true;
}

void HackRfDevice::on_hackrf_rx(const int8_t* raw_bytes, size_t length) {
    if (!rx_callback_ || !raw_bytes || length < 2) return;

    size_t num_samples = length / 2;
    std::vector<ComplexSample> samples(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        float i_val = static_cast<float>(raw_bytes[2 * i]) / 128.0f;
        float q_val = static_cast<float>(raw_bytes[2 * i + 1]) / 128.0f;
        samples[i] = ComplexSample(i_val, q_val);
    }

    rx_callback_(samples.data(), num_samples, current_freq_hz_, current_rate_sps_);
}

} // namespace discan
