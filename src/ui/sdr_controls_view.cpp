#include "ui/sdr_controls_view.hpp"
#include "sdr/sdr_manager.hpp"
#include "core/device_tracker.hpp"
#include "core/ids_engine.hpp"
#include "core/packet_storage.hpp"
#include "ui/theme.hpp"
#include "imgui.h"
#include <iomanip>
#include <sstream>

namespace discan {

void SdrControlsView::render() {
    auto& sdr = SdrManager::instance();
    auto& config = sdr.get_config();
    ISdrDevice* dev = sdr.get_active_device();
    ProtocolScanMode current_mode = sdr.get_protocol_scan_mode();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));

    // Top Header / Banner
    ImGui::BeginChild("SdrControlsPanel", ImVec2(0, 135), true);

    // =========================================================================
    // ROW 1: SDR Hardware Selection + Status + Master Capture & Export Buttons
    // =========================================================================
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.00f, 0.82f, 1.00f, 1.0f), "SDR HARDWARE:");
    ImGui::SameLine();

    const char* sdr_options[] = { "Synthetic Simulator / PCAP", "HackRF One", "Ettus USRP B210" };
    int current_sdr_idx = static_cast<int>(sdr.get_active_device_type());

    ImGui::SetNextItemWidth(220);
    if (ImGui::Combo("##SdrDeviceCombo", &current_sdr_idx, sdr_options, 3)) {
        sdr.switch_device(static_cast<SdrDeviceType>(current_sdr_idx));
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Device Status Badge
    SdrState state = dev ? dev->get_state() : SdrState::DISCONNECTED;
    ImVec4 status_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    const char* status_text = "DISCONNECTED";

    if (state == SdrState::STREAMING) {
        status_color = ImVec4(0.1f, 0.9f, 0.3f, 1.0f);
        status_text = "[ STREAMING ]";
    } else if (state == SdrState::READY) {
        status_color = ImVec4(0.2f, 0.7f, 1.0f, 1.0f);
        status_text = "[ READY ]";
    } else if (state == SdrState::ERROR_STATE) {
        status_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        status_text = "[ ERROR / NOT FOUND ]";
    }

    ImGui::TextColored(status_color, "%s", status_text);
    ImGui::SameLine();
    if (dev) {
        ImGui::TextDisabled("(%s)", dev->get_status_message().c_str());
    }

    // Right-aligned Capture / Action Buttons
    float right_margin = 440.0f;
    float avail_w = ImGui::GetContentRegionAvail().x;
    if (avail_w > right_margin) {
        ImGui::SameLine(ImGui::GetWindowWidth() - right_margin);
    } else {
        ImGui::SameLine();
    }

    bool streaming = sdr.is_streaming();
    if (!streaming) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.60f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.75f, 0.32f, 1.0f));
        if (ImGui::Button(" START CAPTURE ", ImVec2(130, 0))) {
            sdr.start_capture();
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.22f, 0.22f, 1.0f));
        if (ImGui::Button(" STOP CAPTURE ", ImVec2(130, 0))) {
            sdr.stop_capture();
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Data")) {
        DeviceTracker::instance().clear();
        IdsEngine::instance().clear_alerts();
        PacketStorage::instance().clear();
    }

    ImGui::SameLine();
    if (ImGui::Button("Export PCAP")) {
        PacketStorage::instance().export_to_pcap("capture.pcap");
    }

    ImGui::SameLine();
    if (ImGui::Button("Export JSON")) {
        PacketStorage::instance().export_to_json("capture.json");
    }

    ImGui::Separator();

    // =========================================================================
    // ROW 2: DEDICATED PROTOCOL MODE SELECTOR BUTTONS
    // =========================================================================
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "SCAN PROTOCOL:");
    ImGui::SameLine();

    // Helper macro/lambda for protocol mode buttons
    auto mode_button = [&](const char* label, ProtocolScanMode mode, ImVec4 accent_color) {
        bool is_active = (current_mode == mode);
        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, accent_color);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text on active
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.17f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, accent_color);
        }

        if (ImGui::Button(label)) {
            sdr.set_protocol_scan_mode(mode);
        }

        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    };

    // 1. Wi-Fi Button
    mode_button(" Wi-Fi 2.4G (Ch 1, 6, 11) ", ProtocolScanMode::WIFI_2G4_PRIMARY, Theme::get_protocol_color(ProtocolType::WIFI));
    mode_button(" Wi-Fi 2.4G (All 14 Ch) ", ProtocolScanMode::WIFI_2G4_ALL, Theme::get_protocol_color(ProtocolType::WIFI));
    mode_button(" Wi-Fi 5 GHz (UNII 1-3) ", ProtocolScanMode::WIFI_5G_UNII, Theme::get_protocol_color(ProtocolType::WIFI));

    // 2. BLE Button
    mode_button(" Bluetooth BLE (Ch 37, 38, 39) ", ProtocolScanMode::BLE_ALL_ADV, Theme::get_protocol_color(ProtocolType::BLUETOOTH));

    // 3. Zigbee Button
    mode_button(" Zigbee 802.15.4 (All 16 Ch) ", ProtocolScanMode::ZIGBEE_ALL_16_CH, Theme::get_protocol_color(ProtocolType::ZIGBEE));

    // 4. LoRa Button
    mode_button(" LoRa EU868 Wideband ", ProtocolScanMode::LORA_EU868_WIDE, Theme::get_protocol_color(ProtocolType::LORA));
    mode_button(" LoRa US915 (All 64 Ch) ", ProtocolScanMode::LORA_US915_ALL, Theme::get_protocol_color(ProtocolType::LORA));

    // 5. Full Spectrum Sweep Button
    mode_button(" [ Full Spectrum Sweep ] ", ProtocolScanMode::FULL_SPECTRUM_SWEEP, ImVec4(0.2f, 0.95f, 0.5f, 1.0f));

    ImGui::NewLine();

    // =========================================================================
    // ROW 3: LIVE ACTIVE CHANNEL STATUS, DWELL PROGRESS & CONTROLS
    // =========================================================================
    HopChannel active_ch = sdr.get_active_hop_channel();
    const auto& schedule = sdr.get_hop_schedule();
    size_t cur_idx = sdr.get_current_hop_index();

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Theme::get_protocol_color(active_ch.protocol), "▶ ACTIVE CHANNEL:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "[ %s ]", active_ch.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(Hop %lu of %lu | Rate: %.1f MSPS)", (schedule.empty() ? 1 : cur_idx + 1), std::max<size_t>(1, schedule.size()), active_ch.sample_rate_sps / 1e6);

    // Live Dwell Progress Bar
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    float dwell_prog = sdr.get_dwell_progress();
    char prog_str[32];
    std::snprintf(prog_str, sizeof(prog_str), "%.0f ms", sdr.get_dwell_time_ms());
    ImGui::ProgressBar(dwell_prog, ImVec2(90, 0), prog_str);

    // Dwell Time Slider
    ImGui::SameLine();
    ImGui::Text("Dwell:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    float dwell_ms = static_cast<float>(sdr.get_dwell_time_ms());
    if (ImGui::SliderFloat("##DwellSlider", &dwell_ms, 100.0f, 3000.0f, "%.0f ms")) {
        sdr.set_dwell_time_ms(dwell_ms);
    }

    // Hopping Pause / Step Controls
    ImGui::SameLine();
    bool hopping = sdr.is_auto_hopping();
    if (ImGui::Button(hopping ? " ⏸ Pause " : " ▶ Resume ")) {
        sdr.set_auto_hopping(!hopping);
    }

    ImGui::SameLine();
    if (ImGui::Button(" < Prev ")) {
        sdr.step_prev_channel();
    }

    ImGui::SameLine();
    if (ImGui::Button(" Next > ")) {
        sdr.step_next_channel();
    }

    // Gains (HackRF / USRP)
    if (sdr.get_active_device_type() == SdrDeviceType::HACKRF_ONE) {
        ImGui::SameLine();
        ImGui::Text("| LNA:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        int lna = config.hackrf_lna_gain;
        if (ImGui::SliderInt("##LnaGain", &lna, 0, 40, "%ddB")) {
            sdr.set_hackrf_gains(lna, config.hackrf_vga_gain, config.hackrf_amp_enable);
        }

        ImGui::SameLine();
        ImGui::Text("VGA:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        int vga = config.hackrf_vga_gain;
        if (ImGui::SliderInt("##VgaGain", &vga, 0, 62, "%ddB")) {
            sdr.set_hackrf_gains(config.hackrf_lna_gain, vga, config.hackrf_amp_enable);
        }

        ImGui::SameLine();
        bool amp = config.hackrf_amp_enable;
        if (ImGui::Checkbox("+14dB Amp", &amp)) {
            sdr.set_hackrf_gains(config.hackrf_lna_gain, config.hackrf_vga_gain, amp);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

} // namespace discan
