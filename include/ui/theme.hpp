#pragma once

#include "imgui.h"
#include "common/types.hpp"

namespace discan {

class Theme {
public:
    static void apply_dark_cyber_theme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Visual Geometry & Ergonomics
        style.WindowRounding = 6.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.5f;
        style.PopupBorderSize = 1.0f;
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);

        // Cyberpunk Dark Color Palette
        colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.55f, 0.62f, 1.00f);
        colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.11f, 0.15f, 1.00f);
        colors[ImGuiCol_PopupBg]               = ImVec4(0.09f, 0.10f, 0.14f, 0.98f);
        colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.23f, 0.30f, 0.70f);
        colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]               = ImVec4(0.13f, 0.15f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.18f, 0.21f, 0.28f, 1.00f);
        colors[ImGuiCol_FrameBgActive]         = ImVec4(0.22f, 0.26f, 0.35f, 1.00f);
        colors[ImGuiCol_TitleBg]               = ImVec4(0.07f, 0.08f, 0.11f, 1.00f);
        colors[ImGuiCol_TitleBgActive]         = ImVec4(0.11f, 0.13f, 0.18f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
        colors[ImGuiCol_MenuBarBg]             = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.12f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.24f, 0.32f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.26f, 0.31f, 0.42f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.32f, 0.38f, 0.52f, 1.00f);
        colors[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.82f, 1.00f, 1.00f); // Cyan accent
        colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.75f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.20f, 0.88f, 1.00f, 1.00f);
        colors[ImGuiCol_Button]                = ImVec4(0.14f, 0.18f, 0.26f, 1.00f);
        colors[ImGuiCol_ButtonHovered]         = ImVec4(0.20f, 0.27f, 0.38f, 1.00f);
        colors[ImGuiCol_ButtonActive]          = ImVec4(0.25f, 0.35f, 0.50f, 1.00f);
        colors[ImGuiCol_Header]                = ImVec4(0.15f, 0.19f, 0.28f, 1.00f);
        colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.28f, 0.40f, 1.00f);
        colors[ImGuiCol_HeaderActive]          = ImVec4(0.28f, 0.36f, 0.52f, 1.00f);
        colors[ImGuiCol_Separator]             = ImVec4(0.18f, 0.22f, 0.28f, 0.80f);
        colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.00f, 0.75f, 0.95f, 0.78f);
        colors[ImGuiCol_SeparatorActive]       = ImVec4(0.00f, 0.82f, 1.00f, 1.00f);
        colors[ImGuiCol_ResizeGrip]            = ImVec4(0.15f, 0.19f, 0.28f, 0.50f);
        colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.00f, 0.75f, 0.95f, 0.78f);
        colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.00f, 0.82f, 1.00f, 1.00f);
        colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.13f, 0.18f, 1.00f);
        colors[ImGuiCol_TabHovered]            = ImVec4(0.20f, 0.27f, 0.38f, 1.00f);
        colors[ImGuiCol_TabActive]             = ImVec4(0.16f, 0.22f, 0.32f, 1.00f);
        colors[ImGuiCol_TabUnfocused]          = ImVec4(0.09f, 0.10f, 0.14f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.13f, 0.16f, 0.22f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.12f, 0.14f, 0.19f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.22f, 0.26f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]      = ImVec4(0.15f, 0.18f, 0.24f, 1.00f);
        colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    }

    // Protocol Color Palette
    static ImVec4 get_protocol_color(ProtocolType proto) {
        switch (proto) {
            case ProtocolType::WIFI:
                return ImVec4(0.00f, 0.82f, 1.00f, 1.00f); // Bright Cyan
            case ProtocolType::BLUETOOTH:
                return ImVec4(0.23f, 0.53f, 1.00f, 1.00f); // Royal Blue
            case ProtocolType::ZIGBEE:
                return ImVec4(1.00f, 0.75f, 0.04f, 1.00f); // Amber / Yellow
            case ProtocolType::LORA:
                return ImVec4(0.68f, 0.35f, 1.00f, 1.00f); // Neon Purple
            default:
                return ImVec4(0.60f, 0.65f, 0.70f, 1.00f); // Muted Gray
        }
    }

    static ImVec4 get_severity_color(AlertSeverity sev) {
        switch (sev) {
            case AlertSeverity::INFO:     return ImVec4(0.30f, 0.70f, 1.00f, 1.00f);
            case AlertSeverity::LOW:      return ImVec4(0.20f, 0.85f, 0.40f, 1.00f);
            case AlertSeverity::MEDIUM:   return ImVec4(1.00f, 0.75f, 0.10f, 1.00f);
            case AlertSeverity::HIGH:     return ImVec4(1.00f, 0.40f, 0.20f, 1.00f);
            case AlertSeverity::CRITICAL: return ImVec4(1.00f, 0.15f, 0.25f, 1.00f);
            default:                      return ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
        }
    }
};

} // namespace discan
