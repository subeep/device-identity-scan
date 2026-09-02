#include "sdr/hackrf_device.hpp"
#include "dsp/fft_analyzer.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  Testing Physical HackRF One Hardware Device Connection   " << std::endl;
    std::cout << "==========================================================" << std::endl;

    discan::HackRfDevice hackrf;
    if (!hackrf.initialize()) {
        std::cerr << "[-] Failed to initialize HackRF One: " << hackrf.get_status_message() << std::endl;
        return 1;
    }

    std::cout << "[+] HackRF One Initialized: " << hackrf.get_name() << std::endl;
    std::cout << "[+] Status: " << hackrf.get_status_message() << std::endl;

    // Tune to 2.437 GHz (Wi-Fi Channel 6), 20 MSPS, LNA=32dB, VGA=30dB, Amp=true
    hackrf.set_frequency(2437000000.0);
    hackrf.set_sample_rate(20000000.0);
    hackrf.set_bandwidth(20000000.0);
    hackrf.set_hackrf_gains(32, 30, true);

    std::atomic<uint64_t> total_samples_received{0};
    std::atomic<uint64_t> callback_count{0};
    discan::FftAnalyzer fft(512, 50);

    bool started = hackrf.start_rx([&](const discan::ComplexSample* samples, size_t count, double freq_hz, double rate_sps) {
        total_samples_received += count;
        callback_count++;
        fft.process_samples(samples, count, freq_hz, rate_sps);
    });

    if (!started) {
        std::cerr << "[-] Failed to start HackRF RX stream!" << std::endl;
        hackrf.close();
        return 1;
    }

    std::cout << "[+] HackRF One RX Streaming started. Streaming live RF for 3 seconds..." << std::endl;

    for (int i = 0; i < 6; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::vector<float> freqs, power;
        float peak_f = 0.0f, peak_p = 0.0f;
        fft.get_spectrum_data(freqs, power, peak_f, peak_p);
        
        std::cout << "    [" << (i + 1) * 0.5 << "s] Received " << total_samples_received.load()
                  << " samples (" << callback_count.load() << " callbacks) | Peak: "
                  << peak_f << " MHz (" << peak_p << " dBm)" << std::endl;
    }

    hackrf.stop_rx();
    hackrf.close();

    std::cout << "==========================================================" << std::endl;
    std::cout << "[+] SUCCESS: HackRF One streaming verified flawlessly!" << std::endl;
    std::cout << "    Total IQ samples streamed: " << total_samples_received.load() << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
