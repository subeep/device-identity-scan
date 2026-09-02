#include "ui/spectrum_view.hpp"
#include "sdr/sdr_manager.hpp"
#include "imgui.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace discan {

bool SpectrumView::show_waterfall_ = true;
float SpectrumView::floor_dbm_ = -90.0f;
float SpectrumView::ceiling_dbm_ = -20.0f;

static ImVec4 colormap_turbo(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    // Smooth colormap from dark blue -> cyan -> yellow -> red
    float r = std::clamp(1.5f * t - 0.2f, 0.0f, 1.0f);
    float g = std::clamp(1.8f * t * (1.0f - t) * 2.0f, 0.0f, 1.0f);
    if (t > 0.5f) g = std::max(g, std::clamp(2.0f * (1.0f - t), 0.0f, 1.0f));
    float b = std::clamp(1.2f * (1.0f - 1.5f * t), 0.0f, 1.0f);
    return ImVec4(r, g, b, 1.0f);
}

void SpectrumView::render() {
    auto& sdr = SdrManager::instance();
    auto& fft = sdr.get_fft_analyzer();

    std::vector<float> freqs_mhz;
    std::vector<float> power_dbm;
    float peak_freq = 0.0f;
    float peak_pwr = -100.0f;

    fft.get_spectrum_data(freqs_mhz, power_dbm, peak_freq, peak_pwr);

    ImGui::BeginChild("SpectrumContainer", ImVec2(0, 0), true);

    // Spectrum Header info
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.0f, 0.82f, 1.0f, 1.0f), "REAL-TIME RF SPECTRUM & WATERFALL");
    ImGui::SameLine();
    ImGui::TextDisabled("| Peak: %.3f MHz (%.1f dBm)", peak_freq, peak_pwr);

    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    ImGui::Checkbox("Waterfall", &show_waterfall_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##FloorDbm", &floor_dbm_, -120.0f, -50.0f, "Floor: %.0f");

    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float spectrum_height = show_waterfall_ ? (avail.y * 0.55f) : avail.y;
    float waterfall_height = show_waterfall_ ? (avail.y * 0.45f) : 0.0f;

    // Draw FFT Power Spectrum
    if (!power_dbm.empty() && spectrum_height > 40.0f) {
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImVec2(avail.x, spectrum_height);
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(14, 17, 24, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(40, 48, 65, 255));

        // Horizontal Grid lines (dBm steps)
        for (float dbm = -20.0f; dbm >= -100.0f; dbm -= 20.0f) {
            float norm_y = (dbm - floor_dbm_) / (ceiling_dbm_ - floor_dbm_);
            float y = canvas_p1.y - norm_y * canvas_sz.y;
            if (y >= canvas_p0.y && y <= canvas_p1.y) {
                draw_list->AddLine(ImVec2(canvas_p0.x, y), ImVec2(canvas_p1.x, y), IM_COL32(30, 36, 48, 255));
                char dbm_str[16];
                std::snprintf(dbm_str, sizeof(dbm_str), "%.0f dBm", dbm);
                draw_list->AddText(ImVec2(canvas_p0.x + 4, y - 12), IM_COL32(100, 115, 135, 255), dbm_str);
            }
        }

        // Draw FFT Curve
        size_t n_pts = power_dbm.size();
        float x_step = canvas_sz.x / static_cast<float>(n_pts - 1);

        for (size_t i = 0; i + 1 < n_pts; ++i) {
            float norm_y0 = std::clamp((power_dbm[i] - floor_dbm_) / (ceiling_dbm_ - floor_dbm_), 0.0f, 1.0f);
            float norm_y1 = std::clamp((power_dbm[i + 1] - floor_dbm_) / (ceiling_dbm_ - floor_dbm_), 0.0f, 1.0f);

            ImVec2 pt0(canvas_p0.x + i * x_step, canvas_p1.y - norm_y0 * canvas_sz.y);
            ImVec2 pt1(canvas_p0.x + (i + 1) * x_step, canvas_p1.y - norm_y1 * canvas_sz.y);

            // Shaded under curve
            draw_list->AddTriangleFilled(pt0, pt1, ImVec2(pt1.x, canvas_p1.y), IM_COL32(0, 180, 240, 40));
            draw_list->AddTriangleFilled(pt0, ImVec2(pt1.x, canvas_p1.y), ImVec2(pt0.x, canvas_p1.y), IM_COL32(0, 180, 240, 40));

            // Line
            draw_list->AddLine(pt0, pt1, IM_COL32(0, 210, 255, 255), 1.5f);
        }

        ImGui::Dummy(canvas_sz);
    }

    // Draw Waterfall Spectrogram
    if (show_waterfall_ && waterfall_height > 30.0f) {
        std::vector<float> wf_data;
        size_t rows = 0, cols = 0;
        fft.get_waterfall_data(wf_data, rows, cols);

        if (!wf_data.empty() && cols > 0 && rows > 0) {
            ImVec2 wf_p0 = ImGui::GetCursorScreenPos();
            ImVec2 wf_sz = ImVec2(avail.x, waterfall_height);
            ImVec2 wf_p1 = ImVec2(wf_p0.x + wf_sz.x, wf_p0.y + wf_sz.y);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(wf_p0, wf_p1, IM_COL32(10, 12, 16, 255));

            float block_w = wf_sz.x / static_cast<float>(cols);
            float block_h = wf_sz.y / static_cast<float>(rows);

            for (size_t r = 0; r < rows; ++r) {
                float y = wf_p0.y + r * block_h;
                for (size_t c = 0; c < cols; ++c) {
                    float val = wf_data[r * cols + c];
                    float t = std::clamp((val - floor_dbm_) / (ceiling_dbm_ - floor_dbm_), 0.0f, 1.0f);
                    ImVec4 col = colormap_turbo(t);
                    ImU32 ucol = ImGui::ColorConvertFloat4ToU32(col);

                    float x = wf_p0.x + c * block_w;
                    draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + block_w + 1.0f, y + block_h + 1.0f), ucol);
                }
            }
            ImGui::Dummy(wf_sz);
        }
    }

    ImGui::EndChild();
}

} // namespace discan
