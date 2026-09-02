#include "sdr/sdr_manager.hpp"
#include "ui/ui_manager.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <csignal>
#include <cstring>

static discan::UiManager* g_ui_manager = nullptr;

static void sigint_handler(int) {
    std::cout << "\nCaught SIGINT, shutting down gracefully...\n";
    if (g_ui_manager) {
        // Trigger window close
    }
    discan::SdrManager::instance().shutdown();
    std::exit(0);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigint_handler);

    DISCAN_LOG_INFO("===================================================================");
    DISCAN_LOG_INFO("   Device Identity Scan Engine (Multi-Protocol SDR Sniffer)       ");
    DISCAN_LOG_INFO("   Supporting: Wi-Fi 802.11, Bluetooth BLE, Zigbee 802.15.4, LoRa  ");
    DISCAN_LOG_INFO("   Hardware Frontends: HackRF One | USRP B210 | Synthetic Engine   ");
    DISCAN_LOG_INFO("===================================================================");

    auto& sdr = discan::SdrManager::instance();
    sdr.initialize();

    // Parse CLI options
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hackrf") == 0) {
            sdr.switch_device(discan::SdrDeviceType::HACKRF_ONE);
        } else if (std::strcmp(argv[i], "--usrp") == 0) {
            sdr.switch_device(discan::SdrDeviceType::USRP_B210);
        } else if (std::strcmp(argv[i], "--simulated") == 0) {
            sdr.switch_device(discan::SdrDeviceType::SIMULATED);
        } else if (std::strcmp(argv[i], "--freq") == 0 && i + 1 < argc) {
            double freq_mhz = std::stod(argv[++i]);
            sdr.set_frequency(freq_mhz * 1e6);
        } else if (std::strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            double rate_msps = std::stod(argv[++i]);
            sdr.set_sample_rate(rate_msps * 1e6);
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --hackrf         Start with HackRF One SDR hardware\n"
                      << "  --usrp           Start with Ettus USRP B210 SDR hardware\n"
                      << "  --simulated      Start in Synthetic RF & Protocol Simulator mode (default)\n"
                      << "  --freq <MHz>     Initial center frequency in MHz (e.g. 2437.0)\n"
                      << "  --rate <MSPS>    Initial sample rate in MSPS (e.g. 20.0)\n"
                      << "  --help, -h       Display this help message\n";
            return 0;
        }
    }

    // Auto-start capture
    sdr.start_capture();

    // Start Desktop GUI
    discan::UiManager ui;
    g_ui_manager = &ui;

    if (!ui.initialize("Device Identity Scan Engine - Multi-Protocol SDR Sniffer (Wi-Fi / BLE / Zigbee / LoRa)", 1500, 920)) {
        DISCAN_LOG_ERROR("Failed to initialize UI. Exiting.");
        sdr.shutdown();
        return 1;
    }

    ui.run();

    sdr.shutdown();
    DISCAN_LOG_INFO("Exited cleanly.");
    return 0;
}
