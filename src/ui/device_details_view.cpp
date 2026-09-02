#include "ui/device_details_view.hpp"
#include "ui/theme.hpp"
#include "imgui.h"
#include <iomanip>
#include <sstream>

namespace discan {

void DeviceDetailsView::render(const DevicePtr& dev) {
    ImGui::BeginChild("DeviceDetailsContainer", ImVec2(0, 0), true);

    if (!dev) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.4f);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.3f);
        ImGui::TextDisabled("Select a device from the registry table to inspect its preamble and unencrypted headers.");
        ImGui::EndChild();
        return;
    }

    // Header Card
    ImVec4 proto_col = Theme::get_protocol_color(dev->protocol);
    ImGui::TextColored(proto_col, "[ %s ]", protocol_to_string(dev->protocol));
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", dev->device_key.c_str());
    if (!dev->display_name.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 0.82f, 1.0f, 1.0f), "(\"%s\")", dev->display_name.c_str());
    }

    ImGui::Separator();

    // Summary Metrics Columns
    ImGui::Columns(4, "DeviceMetricsCols", false);
    ImGui::TextDisabled("Vendor / OUI:");
    ImGui::Text("%s", dev->manufacturer.c_str());
    ImGui::NextColumn();

    ImGui::TextDisabled("Channel / Freq:");
    if (dev->primary_channel > 0) {
        ImGui::Text("Ch %d (%.1f MHz)", dev->primary_channel, dev->last_frequency_hz / 1e6);
    } else {
        ImGui::Text("%.3f MHz", dev->last_frequency_hz / 1e6);
    }
    ImGui::NextColumn();

    ImGui::TextDisabled("Signal Strength:");
    ImGui::Text("%.0f dBm (Min: %.0f, Max: %.0f)", dev->current_rssi, dev->min_rssi, dev->max_rssi);
    ImGui::NextColumn();

    ImGui::TextDisabled("Total Packets / Bytes:");
    ImGui::Text("%lu pkts (%lu bytes)", dev->total_packets, dev->total_bytes);
    ImGui::Columns(1);

    // RSSI Sparkline History
    if (!dev->rssi_history.empty()) {
        std::vector<float> hist_arr(dev->rssi_history.begin(), dev->rssi_history.end());
        ImGui::PlotLines("##RssiSparkline", hist_arr.data(), static_cast<int>(hist_arr.size()), 0, "Signal History (dBm)", -100.0f, -30.0f, ImVec2(0, 45));
    }

    ImGui::Separator();

    // Sub-views: Tab Bar
    if (ImGui::BeginTabBar("InspectorTabBar")) {
        // Dedicated Kismet Wi-Fi Parameters Tab (if Wi-Fi)
        if (dev->protocol == ProtocolType::WIFI) {
            if (ImGui::BeginTabItem("📶 Wi-Fi Parameters (Kismet View)")) {
                ImGui::BeginChild("WifiKismetScroll", ImVec2(0, 0), false);
                render_wifi_kismet_view(dev);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }

        if (ImGui::BeginTabItem("Preamble & Unencrypted Header Tree")) {
            if (dev->latest_packet && !dev->latest_packet->dissection_tree.empty()) {
                ImGui::BeginChild("TreeScroll", ImVec2(0, 0), false);
                render_dissection_tree(dev->latest_packet->dissection_tree);
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No packet dissection available.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Raw Hex & ASCII Dissector")) {
            if (dev->latest_packet && !dev->latest_packet->raw_data.empty()) {
                ImGui::BeginChild("HexScroll", ImVec2(0, 0), false);
                render_hex_view(dev->latest_packet->raw_data);
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("No raw packet data available.");
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

void DeviceDetailsView::render_wifi_kismet_view(const DevicePtr& dev) {
    if (!dev) return;

    auto pkt = dev->latest_packet;
    const auto& w = pkt ? pkt->wifi : WifiMetadata();

    // Section 1: Network Identity & BSSID
    if (ImGui::CollapsingHeader("🪪 Network Identity & BSSID", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "WifiIdCols", false);
        ImGui::SetColumnWidth(0, 200);

        ImGui::TextDisabled("SSID (Network Name):"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "%s", dev->display_name.c_str());
        if (w.is_hidden_ssid) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[HIDDEN / CLOAKED]");
        }
        ImGui::NextColumn();

        ImGui::TextDisabled("BSSID (AP MAC):"); ImGui::NextColumn();
        ImGui::Text("%s", dev->device_key.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Hardware Manufacturer:"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", dev->manufacturer.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Operating Role:"); ImGui::NextColumn();
        ImGui::Text("%s", w.operating_mode.c_str()); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    // Section 2: RF Radio & PHY Layer Parameters
    if (ImGui::CollapsingHeader("📡 RF Radio & PHY Layer Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "WifiRfCols", false);
        ImGui::SetColumnWidth(0, 200);

        ImGui::TextDisabled("Operating Band:"); ImGui::NextColumn();
        if (dev->primary_channel >= 36) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "5 GHz UNII Band (High Speed / Low Contention)");
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "2.4 GHz ISM Band");
        }
        ImGui::NextColumn();

        ImGui::TextDisabled("Primary Channel:"); ImGui::NextColumn();
        ImGui::Text("Channel %d", dev->primary_channel); ImGui::NextColumn();

        ImGui::TextDisabled("Center Frequency:"); ImGui::NextColumn();
        ImGui::Text("%.3f MHz (%.3f GHz)", dev->last_frequency_hz / 1e6, dev->last_frequency_hz / 1e9); ImGui::NextColumn();

        ImGui::TextDisabled("Channel Bandwidth:"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "%s", w.channel_width.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Signal Strength (RSSI):"); ImGui::NextColumn();
        ImVec4 rssi_color = (dev->current_rssi > -65.0f) ? ImVec4(0.0f, 1.0f, 0.4f, 1.0f) :
                            (dev->current_rssi > -80.0f) ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(rssi_color, "%.1f dBm (Min: %.1f, Max: %.1f, Avg: %.1f dBm)", dev->current_rssi, dev->min_rssi, dev->max_rssi, dev->avg_rssi); ImGui::NextColumn();

        ImGui::TextDisabled("Noise Floor / SNR:"); ImGui::NextColumn();
        float snr = dev->current_rssi - (-95.0f);
        ImGui::Text("-95 dBm (Noise Floor) | Estimated SNR: %.1f dB", snr); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    // Section 3: Security & Cryptography Suite (Kismet Style)
    if (ImGui::CollapsingHeader("🔒 Security, Cryptography & AKM Suite", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "WifiSecCols", false);
        ImGui::SetColumnWidth(0, 200);

        ImGui::TextDisabled("Security Profile:"); ImGui::NextColumn();
        ImVec4 sec_col = (w.encryption.find("WPA3") != std::string::npos) ? ImVec4(0.0f, 1.0f, 0.5f, 1.0f) :
                         (w.encryption.find("WPA2") != std::string::npos) ? ImVec4(0.2f, 0.8f, 1.0f, 1.0f) :
                         (w.encryption.find("Open") != std::string::npos) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
        ImGui::TextColored(sec_col, "%s", w.encryption.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Authentication (AKM):"); ImGui::NextColumn();
        ImGui::Text("%s", w.akm_suite.empty() ? "None / Open" : w.akm_suite.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Pairwise (Unicast) Cipher:"); ImGui::NextColumn();
        ImGui::Text("%s", w.cipher_suite.empty() ? "None" : w.cipher_suite.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Group (Multicast) Cipher:"); ImGui::NextColumn();
        ImGui::Text("%s", w.group_cipher.empty() ? "None" : w.group_cipher.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Protected Mgmt Frames (PMF):"); ImGui::NextColumn();
        if (w.pmf_required) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "PMF Required (802.11w Protected)");
        } else if (w.pmf_capable) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "PMF Capable / Optional (WPA2 Transition)");
        } else {
            ImGui::TextDisabled("PMF Disabled / Not Supported");
        }
        ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    // Section 4: Beacon Timers & Management Parameters
    if (ImGui::CollapsingHeader("⏱️ Beacon Timers & Management Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "WifiTimerCols", false);
        ImGui::SetColumnWidth(0, 200);

        ImGui::TextDisabled("Beacon Interval:"); ImGui::NextColumn();
        ImGui::Text("%d TU (%.2f milliseconds)", w.beacon_interval_tu, w.beacon_interval_tu * 1.024); ImGui::NextColumn();

        ImGui::TextDisabled("DTIM Period & Count:"); ImGui::NextColumn();
        ImGui::Text("DTIM Period: %d | DTIM Count: %d", w.dtim_period, w.dtim_count); ImGui::NextColumn();

        ImGui::TextDisabled("Hardware Clock Timestamp:"); ImGui::NextColumn();
        std::ostringstream ts_ss;
        ts_ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << w.hardware_timestamp << " (" << std::dec << w.hardware_timestamp << " us)";
        ImGui::Text("%s", ts_ss.str().c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Capabilities Bitmask:"); ImGui::NextColumn();
        std::ostringstream cap_ss;
        cap_ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << w.capabilities;
        ImGui::Text("%s", cap_ss.str().c_str()); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    // Section 5: WPS & Vendor Extensions
    if (ImGui::CollapsingHeader("⚙️ WPS (Wi-Fi Protected Setup) & Vendor Extensions", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "WifiWpsCols", false);
        ImGui::SetColumnWidth(0, 200);

        ImGui::TextDisabled("WPS 2.0 Supported:"); ImGui::NextColumn();
        if (w.has_wps || !w.vendor_wps_model.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "YES (WPS 2.0 Active)");
        } else {
            ImGui::TextDisabled("No WPS IE detected");
        }
        ImGui::NextColumn();

        if (w.has_wps || !w.vendor_wps_model.empty()) {
            ImGui::TextDisabled("WPS Setup Lockout:"); ImGui::NextColumn();
            if (w.wps_locked) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "LOCKED (Protected against brute-force)");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "UNLOCKED [Vulnerable to Reaver/PixieDust]");
            }
            ImGui::NextColumn();

            if (!w.vendor_wps_device_name.empty()) {
                ImGui::TextDisabled("WPS Device Name:"); ImGui::NextColumn();
                ImGui::Text("%s", w.vendor_wps_device_name.c_str()); ImGui::NextColumn();
            }

            if (!w.vendor_wps_model.empty()) {
                ImGui::TextDisabled("WPS Model Name:"); ImGui::NextColumn();
                ImGui::Text("%s", w.vendor_wps_model.c_str()); ImGui::NextColumn();
            }

            if (!w.wps_manufacturer.empty()) {
                ImGui::TextDisabled("WPS Manufacturer:"); ImGui::NextColumn();
                ImGui::Text("%s", w.wps_manufacturer.c_str()); ImGui::NextColumn();
            }

            if (!w.wps_serial_number.empty()) {
                ImGui::TextDisabled("WPS Serial Number:"); ImGui::NextColumn();
                ImGui::Text("%s", w.wps_serial_number.c_str()); ImGui::NextColumn();
            }
        }

        if (!w.vendor_specific_ies.empty()) {
            ImGui::TextDisabled("Vendor Specific IEs:"); ImGui::NextColumn();
            std::string ies_str;
            for (size_t i = 0; i < w.vendor_specific_ies.size(); ++i) {
                ies_str += w.vendor_specific_ies[i];
                if (i + 1 < w.vendor_specific_ies.size()) ies_str += ", ";
            }
            ImGui::Text("%s", ies_str.c_str()); ImGui::NextColumn();
        }

        ImGui::Columns(1);
    }

    ImGui::Spacing();

    // Section 6: Supported Rates & PHY Standards
    if (ImGui::CollapsingHeader("⚡ Supported Data Rates & 802.11 Standards", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "WifiRateCols", false);
        ImGui::SetColumnWidth(0, 200);

        ImGui::TextDisabled("PHY Standards:"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "%s", w.phy_standard.c_str()); ImGui::NextColumn();

        ImGui::TextDisabled("Supported Rates (Mbps):"); ImGui::NextColumn();
        if (!w.supported_rates.empty()) {
            std::string rates_str;
            for (size_t i = 0; i < w.supported_rates.size(); ++i) {
                rates_str += w.supported_rates[i];
                if (i + 1 < w.supported_rates.size()) rates_str += ", ";
            }
            ImGui::Text("%s", rates_str.c_str());
        } else {
            ImGui::Text("1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54 Mbps");
        }
        ImGui::NextColumn();

        ImGui::Columns(1);
    }
}

void DeviceDetailsView::render_dissection_tree(const std::vector<DissectedField>& tree) {
    for (size_t i = 0; i < tree.size(); ++i) {
        const auto& field = tree[i];
        
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (field.subfields.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        } else if (i == 0 || i == 1) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        bool node_open = ImGui::TreeNodeEx((void*)(uintptr_t)i, flags, "%s", field.name.c_str());

        if (!field.value.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), ": %s", field.value.c_str());
        }

        if (node_open && !field.subfields.empty()) {
            for (size_t j = 0; j < field.subfields.size(); ++j) {
                const auto& sub = field.subfields[j];
                ImGuiTreeNodeFlags sub_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                ImGui::TreeNodeEx((void*)(uintptr_t)(1000 + i * 100 + j), sub_flags, "%s: %s", sub.name.c_str(), sub.value.c_str());
            }
            ImGui::TreePop();
        }
    }
}

void DeviceDetailsView::render_hex_view(const std::vector<uint8_t>& raw_data) {
    ImGui::TextDisabled("Frame Length: %zu bytes (0x%zX)", raw_data.size(), raw_data.size());
    ImGui::Separator();

    size_t rows = (raw_data.size() + 15) / 16;
    for (size_t r = 0; r < rows; ++r) {
        size_t row_start = r * 16;

        // Offset column
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%04zX: ", row_start);
        ImGui::SameLine();

        // Hex bytes
        std::ostringstream hex_ss;
        for (size_t c = 0; c < 16; ++c) {
            if (row_start + c < raw_data.size()) {
                hex_ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase
                       << static_cast<int>(raw_data[row_start + c]) << " ";
            } else {
                hex_ss << "   ";
            }
            if (c == 7) hex_ss << " ";
        }
        ImGui::TextColored(ImVec4(0.0f, 0.82f, 1.0f, 1.0f), "%s", hex_ss.str().c_str());
        ImGui::SameLine();

        // ASCII representation
        std::string ascii_str;
        for (size_t c = 0; c < 16; ++c) {
            if (row_start + c < raw_data.size()) {
                uint8_t byte = raw_data[row_start + c];
                ascii_str += (byte >= 32 && byte <= 126) ? static_cast<char>(byte) : '.';
            }
        }
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "| %s |", ascii_str.c_str());
    }
}

} // namespace discan
