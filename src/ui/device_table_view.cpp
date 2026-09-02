#include "ui/device_table_view.hpp"
#include "core/device_tracker.hpp"
#include "ui/theme.hpp"
#include "imgui.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace discan {

bool DeviceTableView::show_wifi_ = true;
bool DeviceTableView::show_ble_ = true;
bool DeviceTableView::show_zigbee_ = true;
bool DeviceTableView::show_lora_ = true;
char DeviceTableView::search_filter_[128] = "";

bool DeviceTableView::render(DevicePtr& selected_device) {
    bool device_inspected = false;
    auto& tracker = DeviceTracker::instance();
    auto devices = tracker.get_devices();

    ImGui::BeginChild("DeviceTableContainer", ImVec2(0, 0), true);

    // Filter Bar: Protocol checkboxes + Search input + Device count badges
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "DISCOVERED DEVICES (%lu UNIQUE):", devices.size());
    ImGui::SameLine();

    size_t wifi_cnt = tracker.count_by_protocol(ProtocolType::WIFI);
    size_t ble_cnt = tracker.count_by_protocol(ProtocolType::BLUETOOTH);
    size_t zb_cnt = tracker.count_by_protocol(ProtocolType::ZIGBEE);
    size_t lora_cnt = tracker.count_by_protocol(ProtocolType::LORA);

    std::string wifi_lbl = "Wi-Fi (" + std::to_string(wifi_cnt) + ")";
    std::string ble_lbl = "BLE (" + std::to_string(ble_cnt) + ")";
    std::string zb_lbl = "Zigbee (" + std::to_string(zb_cnt) + ")";
    std::string lora_lbl = "LoRa (" + std::to_string(lora_cnt) + ")";

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::WIFI));
    ImGui::Checkbox(wifi_lbl.c_str(), &show_wifi_);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::BLUETOOTH));
    ImGui::Checkbox(ble_lbl.c_str(), &show_ble_);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::ZIGBEE));
    ImGui::Checkbox(zb_lbl.c_str(), &show_zigbee_);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::get_protocol_color(ProtocolType::LORA));
    ImGui::Checkbox(lora_lbl.c_str(), &show_lora_);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::Text(" | Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##DeviceSearch", search_filter_, sizeof(search_filter_));

    ImGui::SameLine();
    ImGui::TextDisabled("(Click any device to open detailed Inspector Tab)");

    ImGui::Separator();

    // Multi-column Table view with complete decoded information spanning 100% horizontal width
    static ImGuiTableFlags table_flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("DeviceRegistryTable", 9, table_flags)) {
        ImGui::TableSetupColumn("Protocol", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Address / DevEUI", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Device Name / SSID", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Decoded Header / Protocol Info", ImGuiTableColumnFlags_WidthStretch, 300.0f);
        ImGui::TableSetupColumn("Manufacturer / OUI", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Ch / Freq", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn("Signal (RSSI)", ImGuiTableColumnFlags_WidthFixed, 125.0f);
        ImGui::TableSetupColumn("Packets", ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableHeadersRow();

        std::string filter_str = search_filter_;
        std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);

        for (const auto& dev : devices) {
            // Protocol filters
            if (dev->protocol == ProtocolType::WIFI && !show_wifi_) continue;
            if (dev->protocol == ProtocolType::BLUETOOTH && !show_ble_) continue;
            if (dev->protocol == ProtocolType::ZIGBEE && !show_zigbee_) continue;
            if (dev->protocol == ProtocolType::LORA && !show_lora_) continue;

            // Search filter
            if (!filter_str.empty()) {
                std::string match_target = dev->device_key + " " + dev->display_name + " " + dev->manufacturer + " " + dev->decoded_summary;
                std::transform(match_target.begin(), match_target.end(), match_target.begin(), ::tolower);
                if (match_target.find(filter_str) == std::string::npos) {
                    continue;
                }
            }

            ImGui::TableNextRow();

            bool is_selected = (selected_device && selected_device->device_key == dev->device_key);

            // Column 0: Protocol badge (Selectable for the entire row)
            ImGui::TableSetColumnIndex(0);
            ImVec4 proto_col = Theme::get_protocol_color(dev->protocol);
            ImGui::PushStyleColor(ImGuiCol_Text, proto_col);
            std::string sel_label = std::string(protocol_to_short_string(dev->protocol)) + "##" + dev->device_key;
            if (ImGui::Selectable(sel_label.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_device = dev;
                device_inspected = true;
            }
            ImGui::PopStyleColor();

            // Column 1: Hardware MAC / Key
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", dev->device_key.c_str());

            // Column 2: Name / SSID / Model
            ImGui::TableSetColumnIndex(2);
            if (!dev->display_name.empty()) {
                ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "%s", dev->display_name.c_str());
            } else {
                ImGui::TextDisabled("<Unnamed Device>");
            }

            // Column 3: Decoded Protocol & Header Info
            ImGui::TableSetColumnIndex(3);
            if (!dev->decoded_summary.empty()) {
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s", dev->decoded_summary.c_str());
            } else {
                ImGui::TextDisabled("Frame data decoded");
            }

            // Column 4: Manufacturer / OUI
            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", dev->manufacturer.c_str());

            // Column 5: Channel / Frequency
            ImGui::TableSetColumnIndex(5);
            if (dev->primary_channel > 0) {
                ImGui::Text("Ch %d (%.1fM)", dev->primary_channel, dev->last_frequency_hz / 1e6);
            } else {
                ImGui::Text("%.3f MHz", dev->last_frequency_hz / 1e6);
            }

            // Column 6: Signal Strength RSSI
            ImGui::TableSetColumnIndex(6);
            ImVec4 rssi_col = (dev->current_rssi > -65.0f) ? ImVec4(0.0f, 1.0f, 0.4f, 1.0f) :
                              (dev->current_rssi > -80.0f) ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(rssi_col, "%.0f dBm [%.0f/%.0f]", dev->current_rssi, dev->min_rssi, dev->max_rssi);

            // Column 7: Total Packets
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%lu", dev->total_packets);

            // Column 8: Action Inspect Button
            ImGui::TableSetColumnIndex(8);
            std::string btn_id = "Inspect##" + dev->device_key;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.75f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
            if (ImGui::SmallButton(btn_id.c_str())) {
                selected_device = dev;
                device_inspected = true;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    return device_inspected;
}

} // namespace discan
