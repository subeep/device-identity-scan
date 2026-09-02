#pragma once

#include "common/types.hpp"
#include <vector>
#include <mutex>
#include <cmath>

namespace discan {

class FftAnalyzer {
public:
    explicit FftAnalyzer(size_t fft_size = 512, size_t waterfall_history = 100);
    ~FftAnalyzer() = default;

    // Process a block of complex IQ samples
    void process_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps);

    // Retrieve latest FFT power spectrum in dBm (thread-safe copy)
    void get_spectrum_data(std::vector<float>& out_freqs_mhz, std::vector<float>& out_power_dbm, float& out_peak_freq_mhz, float& out_peak_power_dbm);

    // Retrieve waterfall 2D matrix (flattened row-major: [history_rows x fft_size])
    void get_waterfall_data(std::vector<float>& out_waterfall_flat, size_t& out_rows, size_t& out_cols);

    void set_fft_size(size_t size);
    void set_averaging(float alpha); // 0.0 (no smoothing) to 0.95 (heavy smoothing)

private:
    size_t fft_size_;
    size_t waterfall_rows_;
    float smooth_alpha_ = 0.65f;

    std::mutex mutex_;
    std::vector<float> window_;
    std::vector<float> smoothed_power_dbm_;
    std::vector<float> frequencies_mhz_;
    std::vector<float> waterfall_buffer_; // Size = waterfall_rows_ * fft_size_
    
    double current_center_freq_hz_ = 2437000000.0;
    double current_sample_rate_sps_ = 20000000.0;
    float peak_freq_mhz_ = 2437.0f;
    float peak_power_dbm_ = -90.0f;

    void init_window();
    void compute_fft(const std::vector<ComplexSample>& input, std::vector<float>& output_power);
};

} // namespace discan
