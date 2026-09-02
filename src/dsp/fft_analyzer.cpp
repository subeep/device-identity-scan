#include "dsp/fft_analyzer.hpp"
#include <complex>
#include <algorithm>
#include <cstring>

namespace discan {

namespace {
    // Fast Cooley-Tukey Radix-2 in-place FFT
    void cooley_tukey_fft(std::vector<std::complex<float>>& a) {
        size_t n = a.size();
        if (n <= 1) return;

        // Bit-reversal permutation
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(a[i], a[j]);
            }
        }

        // Butterflies
        for (size_t len = 2; len <= n; len <<= 1) {
            float angle = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
            std::complex<float> wlen(std::cos(angle), std::sin(angle));
            for (size_t i = 0; i < n; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                for (size_t j = 0; j < len / 2; ++j) {
                    std::complex<float> u = a[i + j];
                    std::complex<float> v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }
}

FftAnalyzer::FftAnalyzer(size_t fft_size, size_t waterfall_history)
    : fft_size_(fft_size),
      waterfall_rows_(waterfall_history),
      window_(fft_size),
      smoothed_power_dbm_(fft_size, -100.0f),
      frequencies_mhz_(fft_size, 0.0f),
      waterfall_buffer_(waterfall_rows_ * fft_size, -100.0f) {
    init_window();
}

void FftAnalyzer::init_window() {
    for (size_t i = 0; i < fft_size_; ++i) {
        // Hann window
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (fft_size_ - 1)));
    }
}

void FftAnalyzer::set_fft_size(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    fft_size_ = size;
    window_.resize(fft_size_);
    smoothed_power_dbm_.assign(fft_size_, -100.0f);
    frequencies_mhz_.resize(fft_size_);
    waterfall_buffer_.assign(waterfall_rows_ * fft_size_, -100.0f);
    init_window();
}

void FftAnalyzer::set_averaging(float alpha) {
    std::lock_guard<std::mutex> lock(mutex_);
    smooth_alpha_ = std::clamp(alpha, 0.0f, 0.99f);
}

void FftAnalyzer::process_samples(const ComplexSample* samples, size_t count, double center_freq_hz, double sample_rate_sps) {
    if (!samples || count < fft_size_) return;

    std::vector<ComplexSample> input(fft_size_);
    // Apply window to the first fft_size samples
    for (size_t i = 0; i < fft_size_; ++i) {
        input[i] = samples[i] * window_[i];
    }

    // Run FFT
    cooley_tukey_fft(input);

    std::vector<float> power_dbm(fft_size_);
    float max_pwr = -200.0f;
    float max_freq = static_cast<float>(center_freq_hz / 1e6);

    double start_freq_mhz = (center_freq_hz - sample_rate_sps / 2.0) / 1e6;
    double freq_step_mhz = (sample_rate_sps / static_cast<double>(fft_size_)) / 1e6;

    // FFT Shift to center DC
    size_t half = fft_size_ / 2;
    for (size_t i = 0; i < fft_size_; ++i) {
        size_t shifted_idx = (i < half) ? (i + half) : (i - half);
        float mag_sq = std::norm(input[shifted_idx]) / static_cast<float>(fft_size_ * fft_size_);
        float pwr_dbm = 10.0f * std::log10(std::max(mag_sq, 1e-12f)) + 10.0f; // Scale to ~ dBm range
        power_dbm[i] = pwr_dbm;

        float freq_mhz = static_cast<float>(start_freq_mhz + i * freq_step_mhz);

        if (pwr_dbm > max_pwr) {
            max_pwr = pwr_dbm;
            max_freq = freq_mhz;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    current_center_freq_hz_ = center_freq_hz;
    current_sample_rate_sps_ = sample_rate_sps;
    peak_power_dbm_ = max_pwr;
    peak_freq_mhz_ = max_freq;

    // Smooth power spectrum and update frequency axis
    for (size_t i = 0; i < fft_size_; ++i) {
        smoothed_power_dbm_[i] = smoothed_power_dbm_[i] * smooth_alpha_ + power_dbm[i] * (1.0f - smooth_alpha_);
        frequencies_mhz_[i] = static_cast<float>(start_freq_mhz + i * freq_step_mhz);
    }

    // Shift waterfall buffer down by 1 row
    if (waterfall_rows_ > 1) {
        std::memmove(&waterfall_buffer_[fft_size_], &waterfall_buffer_[0], (waterfall_rows_ - 1) * fft_size_ * sizeof(float));
    }
    // Insert new row at index 0
    std::memcpy(&waterfall_buffer_[0], smoothed_power_dbm_.data(), fft_size_ * sizeof(float));
}

void FftAnalyzer::get_spectrum_data(std::vector<float>& out_freqs_mhz, std::vector<float>& out_power_dbm, float& out_peak_freq_mhz, float& out_peak_power_dbm) {
    std::lock_guard<std::mutex> lock(mutex_);
    out_freqs_mhz = frequencies_mhz_;
    out_power_dbm = smoothed_power_dbm_;
    out_peak_freq_mhz = peak_freq_mhz_;
    out_peak_power_dbm = peak_power_dbm_;
}

void FftAnalyzer::get_waterfall_data(std::vector<float>& out_waterfall_flat, size_t& out_rows, size_t& out_cols) {
    std::lock_guard<std::mutex> lock(mutex_);
    out_waterfall_flat = waterfall_buffer_;
    out_rows = waterfall_rows_;
    out_cols = fft_size_;
}

} // namespace discan
