#include "ui/cluster_map_view.hpp"
#include "core/device_tracker.hpp"
#include "ui/theme.hpp"
#include "imgui.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <functional>
#include <sstream>
#include <iomanip>

namespace discan {

ClusterMapMode ClusterMapView::current_mode_ = ClusterMapMode::RADIAL_PROXIMITY;
bool ClusterMapView::show_wifi_ = true;
bool ClusterMapView::show_ble_ = true;
bool ClusterMapView::show_zigbee_ = true;
bool ClusterMapView::show_lora_ = true;
float ClusterMapView::radar_sweep_angle_ = 0.0f;
float ClusterMapView::immediate_threshold_ = -50.0f;
float ClusterMapView::near_threshold_ = -65.0f;
float ClusterMapView::mid_threshold_ = -80.0f;

bool ClusterMapView::render(DevicePtr& selected_device) {
    bool device_clicked = false;
    auto& tracker = DeviceTracker::instance();
    auto all_devices = tracker.get_devices();

    // Filter active devices based on protocol checkboxes
    std::vector<DevicePtr> devices;
    for (const auto& d : all_devices) {
        if (d->protocol == ProtocolType::WIFI && !show_wifi_) continue;
        if (d->protocol == ProtocolType::BLUETOOTH && !show_ble_) continue;
        if (d->protocol == ProtocolType::ZIGBEE && !show_zigbee_) continue;
        if (d->protocol == ProtocolType::LORA && !show_lora_) continue;
        devices.push_back(d);
    }

    ImGui::BeginChild("ClusterMapContainer", ImVec2(0, 0), true);

    // Header Toolbar
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "RSSI SIGNAL PROXIMITY CLUSTER MAP (%zu ACTIVE):", devices.size());
    ImGui::SameLine();

    // Mode Selector
    int mode_int = static_cast<int>(current_mode_);
    ImGui::RadioButton("🌐 Radial Radar Orbit", &mode_int, 0);
    ImGui::SameLine();
    ImGui::RadioButton("📈 2D RSSI vs. Frequency", &mode_int, 1);
    ImGui::SameLine();
    ImGui::RadioButton("📊 Signal Zone Breakdown", &mode_int, 2);
    current_mode_ = static_cast<ClusterMapMode>(mode_int);

    ImGui::SameLine();
    ImGui::TextDisabled("| Protocol Filters:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::WIFI));
    ImGui::Checkbox("Wi-Fi", &show_wifi_);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::BLUETOOTH));
    ImGui::Checkbox("BLE", &show_ble_);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::ZIGBEE));
    ImGui::Checkbox("Zigbee", &show_zigbee_);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::LORA));
    ImGui::Checkbox("LoRa", &show_lora_);
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Render selected visualizer mode
    switch (current_mode_) {
        case ClusterMapMode::RADIAL_PROXIMITY:
            render_radial_proximity(devices, selected_device, device_clicked);
            break;
        case ClusterMapMode::SCATTER_2D:
            render_scatter_2d(devices, selected_device, device_clicked);
            break;
        case ClusterMapMode::ZONE_BREAKDOWN:
            render_zone_breakdown(devices, selected_device, device_clicked);
            break;
    }

    ImGui::EndChild();
    return device_clicked;
}

void ClusterMapView::render_radial_proximity(const std::vector<DevicePtr>& devices, DevicePtr& selected_device, bool& device_clicked) {
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 100.0f || canvas_sz.y < 100.0f) return;

    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background Canvas
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(14, 17, 24, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(35, 42, 58, 255));

    ImVec2 center = ImVec2(canvas_p0.x + canvas_sz.x * 0.5f, canvas_p0.y + canvas_sz.y * 0.5f);
    float max_radius = std::min(canvas_sz.x, canvas_sz.y) * 0.45f;

    // 4 Proximity Rings
    float r_imm = max_radius * 0.25f;
    float r_near = max_radius * 0.50f;
    float r_mid = max_radius * 0.75f;
    float r_far = max_radius * 1.00f;

    // Concentric shaded zone fills
    draw_list->AddCircleFilled(center, r_far, IM_COL32(20, 24, 38, 120), 64);
    draw_list->AddCircleFilled(center, r_mid, IM_COL32(18, 32, 45, 120), 64);
    draw_list->AddCircleFilled(center, r_near, IM_COL32(35, 38, 20, 120), 64);
    draw_list->AddCircleFilled(center, r_imm, IM_COL32(45, 22, 22, 140), 64);

    // Ring borders with glowing styling
    draw_list->AddCircle(center, r_far, IM_COL32(75, 55, 120, 180), 64, 1.5f);
    draw_list->AddCircle(center, r_mid, IM_COL32(0, 180, 220, 180), 64, 1.5f);
    draw_list->AddCircle(center, r_near, IM_COL32(230, 190, 30, 180), 64, 1.5f);
    draw_list->AddCircle(center, r_imm, IM_COL32(240, 70, 70, 200), 64, 2.0f);

    // Crosshairs & Angle Rays
    draw_list->AddLine(ImVec2(center.x - max_radius, center.y), ImVec2(center.x + max_radius, center.y), IM_COL32(50, 60, 80, 130), 1.0f);
    draw_list->AddLine(ImVec2(center.x, center.y - max_radius), ImVec2(center.x, center.y + max_radius), IM_COL32(50, 60, 80, 130), 1.0f);

    // Animated Radar Sweep
    radar_sweep_angle_ += ImGui::GetIO().DeltaTime * 1.2f;
    if (radar_sweep_angle_ > 2.0f * 3.14159265f) radar_sweep_angle_ -= 2.0f * 3.14159265f;

    ImVec2 sweep_end = ImVec2(center.x + max_radius * std::cos(radar_sweep_angle_),
                              center.y + max_radius * std::sin(radar_sweep_angle_));
    draw_list->AddLine(center, sweep_end, IM_COL32(0, 230, 255, 220), 2.0f);

    // Center SDR Receiver Icon
    draw_list->AddCircleFilled(center, 7.0f, IM_COL32(0, 255, 180, 255));
    draw_list->AddCircle(center, 12.0f, IM_COL32(0, 255, 180, 140), 32, 1.5f);
    draw_list->AddText(ImVec2(center.x + 10, center.y - 18), IM_COL32(0, 255, 180, 255), "SDR Receiver");

    // Zone Labels
    draw_list->AddText(ImVec2(center.x + 8, center.y - r_imm + 4), IM_COL32(255, 100, 100, 220), "🔴 IMMEDIATE (<2m | >= -50 dBm)");
    draw_list->AddText(ImVec2(center.x + 8, center.y - r_near + 4), IM_COL32(255, 220, 50, 220), "🟡 NEAR-FIELD (2-5m | -50..-65 dBm)");
    draw_list->AddText(ImVec2(center.x + 8, center.y - r_mid + 4), IM_COL32(0, 210, 255, 220), "🟢 MID-RANGE (5-15m | -65..-80 dBm)");
    draw_list->AddText(ImVec2(center.x + 8, center.y - r_far + 4), IM_COL32(180, 140, 255, 220), "🔵 FAR-FIELD (>15m | < -80 dBm)");

    // Plot Devices
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    DevicePtr hovered_dev = nullptr;
    ImVec2 hovered_pos;

    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& dev = devices[i];

        // Map RSSI (-100 dBm to -30 dBm) to radius
        float rssi = std::clamp(dev->current_rssi, -100.0f, -30.0f);
        float norm_dist = 1.0f - ((rssi - (-100.0f)) / 70.0f); // 0.0 (strongest) to 1.0 (weakest)
        float dev_r = max_radius * (0.12f + norm_dist * 0.83f);

        // Derive consistent angular distribution from device key hash
        size_t hash_val = std::hash<std::string>{}(dev->device_key);
        float angle = static_cast<float>((hash_val % 3600) / 3600.0f * 2.0 * 3.14159265);

        ImVec2 dev_pos = ImVec2(center.x + dev_r * std::cos(angle), center.y + dev_r * std::sin(angle));

        // Color by protocol
        ImVec4 p_col = Theme::get_protocol_color(dev->protocol);
        ImU32 node_col = ImGui::ColorConvertFloat4ToU32(p_col);
        ImU32 aura_col = ImGui::ColorConvertFloat4ToU32(ImVec4(p_col.x, p_col.y, p_col.z, 0.35f));

        bool is_sel = (selected_device && selected_device->device_key == dev->device_key);

        // Draw outer pulse aura
        float dot_radius = is_sel ? 8.0f : 5.5f;
        draw_list->AddCircleFilled(dev_pos, dot_radius + 4.0f, aura_col, 16);
        draw_list->AddCircleFilled(dev_pos, dot_radius, node_col, 16);
        draw_list->AddCircle(dev_pos, dot_radius, IM_COL32(255, 255, 255, 200), 16, 1.5f);

        // Device Label
        std::string disp_name = !dev->display_name.empty() ? dev->display_name : dev->device_key;
        if (disp_name.length() > 14) disp_name = disp_name.substr(0, 12) + "..";
        draw_list->AddText(ImVec2(dev_pos.x + 8, dev_pos.y - 7), IM_COL32(220, 225, 235, 240), disp_name.c_str());

        // Check hover
        float dist_to_mouse = std::sqrt(std::pow(mouse_pos.x - dev_pos.x, 2) + std::pow(mouse_pos.y - dev_pos.y, 2));
        if (dist_to_mouse <= 12.0f) {
            hovered_dev = dev;
            hovered_pos = dev_pos;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                selected_device = dev;
                device_clicked = true;
            }
        }
    }

    // Render Hover Tooltip
    if (hovered_dev) {
        draw_list->AddCircle(hovered_pos, 14.0f, IM_COL32(255, 255, 0, 255), 24, 2.0f);
        
        ImGui::BeginTooltip();
        ImGui::TextColored(Theme::get_protocol_color(hovered_dev->protocol), "[ %s ]", protocol_to_string(hovered_dev->protocol));
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", hovered_dev->display_name.c_str());
        ImGui::Separator();
        ImGui::Text("Hardware Address: %s", hovered_dev->device_key.c_str());
        ImGui::Text("Manufacturer:     %s", hovered_dev->manufacturer.c_str());
        if (hovered_dev->primary_channel > 0) {
            ImGui::Text("Channel / Freq:   Ch %d (%.1f MHz)", hovered_dev->primary_channel, hovered_dev->last_frequency_hz / 1e6);
        } else {
            ImGui::Text("Frequency:        %.3f MHz", hovered_dev->last_frequency_hz / 1e6);
        }
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Current RSSI:     %.0f dBm (Min: %.0f, Max: %.0f)", hovered_dev->current_rssi, hovered_dev->min_rssi, hovered_dev->max_rssi);
        ImGui::Text("Packets Captured: %lu", hovered_dev->total_packets);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "👉 Click node to open Device Inspector tab");
        ImGui::EndTooltip();
    }
}

void ClusterMapView::render_scatter_2d(const std::vector<DevicePtr>& devices, DevicePtr& selected_device, bool& device_clicked) {
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 100.0f || canvas_sz.y < 100.0f) return;

    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(14, 17, 24, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(35, 42, 58, 255));

    float margin_left = 60.0f;
    float margin_bottom = 40.0f;
    float margin_top = 30.0f;
    float margin_right = 30.0f;

    float plot_x0 = canvas_p0.x + margin_left;
    float plot_y0 = canvas_p0.y + margin_top;
    float plot_x1 = canvas_p1.x - margin_right;
    float plot_y1 = canvas_p1.y - margin_bottom;
    float plot_w = plot_x1 - plot_x0;
    float plot_h = plot_y1 - plot_y0;

    // Y-Axis: RSSI from -100 to -20 dBm
    float rssi_min = -100.0f;
    float rssi_max = -20.0f;

    // X-Axis: Channel / Frequency (2400 to 2485 MHz, or 800 to 5850 MHz)
    float freq_min = 2400.0f;
    float freq_max = 2485.0f;

    // Draw horizontal RSSI Grid Lines & Proximity Zones
    for (float r = -100.0f; r <= -20.0f; r += 10.0f) {
        float y = plot_y1 - ((r - rssi_min) / (rssi_max - rssi_min)) * plot_h;
        draw_list->AddLine(ImVec2(plot_x0, y), ImVec2(plot_x1, y), IM_COL32(40, 48, 65, 180));
        
        std::string lbl = std::to_string(static_cast<int>(r)) + " dBm";
        draw_list->AddText(ImVec2(canvas_p0.x + 5, y - 7), IM_COL32(160, 170, 190, 220), lbl.c_str());
    }

    // Draw Zone Threshold Highlights
    float y_imm = plot_y1 - ((-50.0f - rssi_min) / (rssi_max - rssi_min)) * plot_h;
    float y_near = plot_y1 - ((-65.0f - rssi_min) / (rssi_max - rssi_min)) * plot_h;
    float y_mid = plot_y1 - ((-80.0f - rssi_min) / (rssi_max - rssi_min)) * plot_h;

    draw_list->AddLine(ImVec2(plot_x0, y_imm), ImVec2(plot_x1, y_imm), IM_COL32(240, 70, 70, 220), 2.0f);
    draw_list->AddText(ImVec2(plot_x1 - 180, y_imm - 16), IM_COL32(240, 70, 70, 220), "Immediate Threshold (-50 dBm)");

    draw_list->AddLine(ImVec2(plot_x0, y_near), ImVec2(plot_x1, y_near), IM_COL32(230, 190, 30, 220), 1.5f);
    draw_list->AddText(ImVec2(plot_x1 - 180, y_near - 16), IM_COL32(230, 190, 30, 220), "Near-Field Threshold (-65 dBm)");

    draw_list->AddLine(ImVec2(plot_x0, y_mid), ImVec2(plot_x1, y_mid), IM_COL32(0, 180, 220, 220), 1.5f);
    draw_list->AddText(ImVec2(plot_x1 - 180, y_mid - 16), IM_COL32(0, 180, 220, 220), "Mid-Range Threshold (-80 dBm)");

    // Plot Device Scatter Points
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    DevicePtr hovered_dev = nullptr;

    for (const auto& dev : devices) {
        float f_mhz = dev->last_frequency_hz / 1e6;
        if (f_mhz < freq_min || f_mhz > freq_max) f_mhz = 2437.0f; // fallback for 2.4G display

        float x = plot_x0 + ((f_mhz - freq_min) / (freq_max - freq_min)) * plot_w;
        float y = plot_y1 - ((std::clamp(dev->current_rssi, rssi_min, rssi_max) - rssi_min) / (rssi_max - rssi_min)) * plot_h;

        ImVec4 p_col = Theme::get_protocol_color(dev->protocol);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(p_col);

        draw_list->AddCircleFilled(ImVec2(x, y), 6.0f, col, 16);
        draw_list->AddCircle(ImVec2(x, y), 6.0f, IM_COL32(255, 255, 255, 200), 16, 1.0f);

        std::string d_name = !dev->display_name.empty() ? dev->display_name : dev->device_key;
        if (d_name.length() > 12) d_name = d_name.substr(0, 10) + "..";
        draw_list->AddText(ImVec2(x + 7, y - 6), IM_COL32(220, 225, 235, 240), d_name.c_str());

        float dist = std::sqrt(std::pow(mouse_pos.x - x, 2) + std::pow(mouse_pos.y - y, 2));
        if (dist <= 10.0f) {
            hovered_dev = dev;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                selected_device = dev;
                device_clicked = true;
            }
        }
    }

    if (hovered_dev) {
        ImGui::BeginTooltip();
        ImGui::TextColored(Theme::get_protocol_color(hovered_dev->protocol), "%s", hovered_dev->display_name.c_str());
        ImGui::Text("MAC: %s | RSSI: %.0f dBm | Freq: %.1f MHz", hovered_dev->device_key.c_str(), hovered_dev->current_rssi, hovered_dev->last_frequency_hz / 1e6);
        ImGui::EndTooltip();
    }
}

void ClusterMapView::render_zone_breakdown(const std::vector<DevicePtr>& devices, DevicePtr& selected_device, bool& device_clicked) {
    size_t imm_cnt = 0, near_cnt = 0, mid_cnt = 0, far_cnt = 0;
    float imm_max = -120.0f, near_max = -120.0f, mid_max = -120.0f, far_max = -120.0f;

    for (const auto& dev : devices) {
        if (dev->current_rssi >= immediate_threshold_) {
            imm_cnt++;
            imm_max = std::max(imm_max, dev->current_rssi);
        } else if (dev->current_rssi >= near_threshold_) {
            near_cnt++;
            near_max = std::max(near_max, dev->current_rssi);
        } else if (dev->current_rssi >= mid_threshold_) {
            mid_cnt++;
            mid_max = std::max(mid_max, dev->current_rssi);
        } else {
            far_cnt++;
            far_max = std::max(far_max, dev->current_rssi);
        }
    }

    // 4 Summary Metric Cards
    ImGui::Columns(4, "ZoneMetricCols", false);

    // Card 1: Immediate
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.3f, 0.3f, 0.8f));
    ImGui::BeginChild("CardImm", ImVec2(0, 90), true);
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "🔴 IMMEDIATE ZONE");
    ImGui::Text("RSSI >= -50 dBm (<2m)");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%zu Devices", imm_cnt);
    if (imm_cnt > 0) ImGui::TextDisabled("Peak: %.0f dBm", imm_max);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::NextColumn();

    // Card 2: Near-Field
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.8f, 0.1f, 0.8f));
    ImGui::BeginChild("CardNear", ImVec2(0, 90), true);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.1f, 1.0f), "🟡 NEAR-FIELD ZONE");
    ImGui::Text("-50 to -65 dBm (2-5m)");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%zu Devices", near_cnt);
    if (near_cnt > 0) ImGui::TextDisabled("Peak: %.0f dBm", near_max);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::NextColumn();

    // Card 3: Mid-Range
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.85f, 1.0f, 0.8f));
    ImGui::BeginChild("CardMid", ImVec2(0, 90), true);
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "🟢 MID-RANGE ZONE");
    ImGui::Text("-65 to -80 dBm (5-15m)");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%zu Devices", mid_cnt);
    if (mid_cnt > 0) ImGui::TextDisabled("Peak: %.0f dBm", mid_max);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::NextColumn();

    // Card 4: Far-Field
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.4f, 1.0f, 0.8f));
    ImGui::BeginChild("CardFar", ImVec2(0, 90), true);
    ImGui::TextColored(ImVec4(0.7f, 0.4f, 1.0f, 1.0f), "🔵 FAR-FIELD ZONE");
    ImGui::Text("< -80 dBm (>15m)");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%zu Devices", far_cnt);
    if (far_cnt > 0) ImGui::TextDisabled("Peak: %.0f dBm", far_max);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();

    // Density Bar
    if (!devices.empty()) {
        float f_imm = static_cast<float>(imm_cnt) / devices.size();
        float f_near = static_cast<float>(near_cnt) / devices.size();
        float f_mid = static_cast<float>(mid_cnt) / devices.size();
        float f_far = static_cast<float>(far_cnt) / devices.size();

        ImGui::TextDisabled("Signal Distribution Bar:");
        ImGui::ProgressBar(f_imm, ImVec2(0, 12), "");
    }
}

} // namespace discan
