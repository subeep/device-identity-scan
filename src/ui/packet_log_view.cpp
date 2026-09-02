#include "ui/packet_log_view.hpp"
#include "core/packet_storage.hpp"
#include "core/ids_engine.hpp"
#include "core/device_tracker.hpp"
#include "ui/theme.hpp"
#include "imgui.h"
#include <iomanip>
#include <sstream>

namespace discan {

bool PacketLogView::auto_scroll_ = true;
bool PacketLogView::show_alerts_only_ = false;

void PacketLogView::render(DevicePtr& selected_device) {
    ImGui::BeginChild("PacketLogContainer", ImVec2(0, 0), true);

    // Top Controls
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.0f, 0.82f, 1.0f, 1.0f), "LIVE CAPTURE STREAM & IDS ALERTS");
    ImGui::SameLine();
    ImGui::TextDisabled("(Stored: %lu packets)", PacketStorage::instance().total_packets_stored());

    ImGui::SameLine(ImGui::GetWindowWidth() - 260);
    ImGui::Checkbox("Auto-Scroll", &auto_scroll_);
    ImGui::SameLine();
    ImGui::Checkbox("Alerts Only", &show_alerts_only_);

    ImGui::Separator();

    // IDS Alerts Section (if any)
    auto alerts = IdsEngine::instance().get_alerts();
    if (!alerts.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.1f, 1.0f), "SECURITY ALERTS (%lu):", alerts.size());
        ImGui::BeginChild("AlertsBanner", ImVec2(0, 65), true);
        for (const auto& al : alerts) {
            ImVec4 col = Theme::get_severity_color(al.severity);
            ImGui::TextColored(col, "[ %s ]", al.title.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", al.source_mac.c_str());
            ImGui::SameLine();
            ImGui::Text(" - %s", al.details.c_str());
        }
        ImGui::EndChild();
        ImGui::Separator();
    }

    if (show_alerts_only_) {
        ImGui::EndChild();
        return;
    }

    // Packet Table
    auto packets = PacketStorage::instance().get_recent_packets(200);

    static ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("PacketLogTable", 7, flags)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Proto", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("Type / Subtype", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("RSSI", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("Dissected Info / Summary", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& pkt : packets) {
            ImGui::TableNextRow();

            // Col 0: ID
            ImGui::TableSetColumnIndex(0);
            char id_str[16];
            std::snprintf(id_str, sizeof(id_str), "#%lu", pkt->packet_id);
            if (ImGui::Selectable(id_str, false, ImGuiSelectableFlags_SpanAllColumns)) {
                auto dev = DeviceTracker::instance().get_device(pkt->source_address);
                if (dev) {
                    selected_device = dev;
                }
            }

            // Col 1: Time
            ImGui::TableSetColumnIndex(1);
            auto in_time_t = std::chrono::system_clock::to_time_t(pkt->timestamp);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(pkt->timestamp.time_since_epoch()) % 1000;
            std::ostringstream ss;
            ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S") << "." << std::setw(2) << std::setfill('0') << (ms.count() / 10);
            ImGui::TextDisabled("%s", ss.str().c_str());

            // Col 2: Proto badge
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(Theme::get_protocol_color(pkt->protocol), "%s", protocol_to_short_string(pkt->protocol));

            // Col 3: Subtype
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", pkt->protocol_subtype.c_str());

            // Col 4: Source
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%s", pkt->source_address.c_str());

            // Col 5: RSSI
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.0f dBm", pkt->rssi_dbm);

            // Col 6: Summary
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(pkt->summary_description.c_str());
        }

        if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

} // namespace discan
