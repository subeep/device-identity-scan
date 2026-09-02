#include "ui/ui_manager.hpp"
#include "ui/theme.hpp"
#include "ui/sdr_controls_view.hpp"
#include "ui/device_table_view.hpp"
#include "ui/device_details_view.hpp"
#include "ui/spectrum_view.hpp"
#include "ui/packet_log_view.hpp"
#include "sdr/sdr_manager.hpp"
#include "common/logger.hpp"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace discan {

static void glfw_error_callback(int error, const char* description) {
    DISCAN_LOG_ERROR("GLFW Error " << error << ": " << description);
}

UiManager::UiManager() = default;

UiManager::~UiManager() {
    shutdown();
}

bool UiManager::initialize(const std::string& window_title, int width, int height) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        DISCAN_LOG_ERROR("Failed to initialize GLFW");
        return false;
    }

    // GL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(width, height, window_title.c_str(), nullptr, nullptr);
    if (!window_) {
        DISCAN_LOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable VSync for smooth 60 FPS

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply Cyber-Dark Theme
    Theme::apply_dark_cyber_theme();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    DISCAN_LOG_INFO("UI Manager initialized successfully (" << width << "x" << height << ")");
    return true;
}

void UiManager::run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_frame();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void UiManager::render_frame() {
    // Fullscreen Root Window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    ImGui::Begin("DeviceIdentityScanWorkspace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // 1. Top Section: SDR Hardware Controls & Capture Toolbar
    SdrControlsView::render();

    // Layout dimensions
    ImVec2 content_avail = ImGui::GetContentRegionAvail();
    float left_width = content_avail.x * 0.54f;
    float right_width = content_avail.x - left_width - 8.0f;
    float total_height = content_avail.y;

    float top_panel_height = total_height * 0.58f;
    float bottom_panel_height = total_height - top_panel_height - 8.0f;

    // 2. Left Column: Device Registry Table (Top) & Real-time RF Spectrum (Bottom)
    ImGui::BeginGroup();
    {
        // Device Table View
        ImGui::BeginChild("LeftTopPanel", ImVec2(left_width, top_panel_height), false);
        DeviceTableView::render(selected_device_);
        ImGui::EndChild();

        // RF Spectrum & Waterfall View
        ImGui::BeginChild("LeftBottomPanel", ImVec2(left_width, bottom_panel_height), false);
        SpectrumView::render();
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    // 3. Right Column: Selected Device Deep Inspector (Top) & Live Packet Stream / Alerts (Bottom)
    ImGui::BeginGroup();
    {
        // Selected Device Deep Inspector
        ImGui::BeginChild("RightTopPanel", ImVec2(right_width, top_panel_height), false);
        DeviceDetailsView::render(selected_device_);
        ImGui::EndChild();

        // Live Packet Stream & IDS Alerts View
        ImGui::BeginChild("RightBottomPanel", ImVec2(right_width, bottom_panel_height), false);
        PacketLogView::render(selected_device_);
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    ImGui::End();
}

void UiManager::shutdown() {
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        DISCAN_LOG_INFO("UI Manager shutdown complete");
    }
}

} // namespace discan
