#include "ui/ui_manager.hpp"
#include "ui/theme.hpp"
#include "ui/sdr_controls_view.hpp"
#include "ui/device_table_view.hpp"
#include "ui/device_details_view.hpp"
#include "ui/cluster_map_view.hpp"
#include "ui/spectrum_view.hpp"
#include "ui/packet_log_view.hpp"
#include "common/logger.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

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

    // GL 3.3 + GLSL 330
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(width, height, window_title.c_str(), nullptr, nullptr);
    if (!window_) {
        DISCAN_LOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply Sleek Dark Cyberpunk / Tactical Glassmorphism Theme
    Theme::apply_dark_cyber_theme();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

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

        // Render main docking layout and sub-views
        render_frame();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.06f, 0.08f, 0.11f, 1.0f);
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

    // 1. Top Section: SDR Hardware Controls & Capture Toolbar (Full Width)
    SdrControlsView::render();

    ImGui::Spacing();

    // 2. Master Workspace Tab Bar (Spanning full horizontal width)
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll;
    if (ImGui::BeginTabBar("MasterWorkspaceTabBar", tab_bar_flags)) {
        
        // Tab 1: Full-Width Devices Registry
        ImGuiTabItemFlags dev_list_flags = 0;
        if (focus_devices_list_) {
            dev_list_flags |= ImGuiTabItemFlags_SetSelected;
            focus_devices_list_ = false;
        }
        if (ImGui::BeginTabItem("📱 Discovered Devices Registry", nullptr, dev_list_flags)) {
            bool clicked = DeviceTableView::render(selected_device_);
            if (clicked && selected_device_) {
                device_tab_open_ = true;
                focus_device_tab_ = true;
            }
            ImGui::EndTabItem();
        }

        // Tab 2: Dynamic Dedicated Device Inspector (Only open/visible when a device is clicked)
        if (device_tab_open_ && selected_device_) {
            ImGuiTabItemFlags inspect_flags = 0;
            if (focus_device_tab_) {
                inspect_flags |= ImGuiTabItemFlags_SetSelected;
                focus_device_tab_ = false;
            }

            std::string dev_title = !selected_device_->display_name.empty() 
                ? selected_device_->display_name 
                : selected_device_->device_key;
            std::string tab_name = "🔍 " + dev_title + " (" + protocol_to_short_string(selected_device_->protocol) + ")###DeviceInspectTab";

            if (ImGui::BeginTabItem(tab_name.c_str(), &device_tab_open_, inspect_flags)) {
                // Top Navigation Bar inside Device Inspector
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.25f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.65f, 0.35f, 1.0f));
                if (ImGui::Button("⬅ Back to Discovered Devices List")) {
                    focus_devices_list_ = true;
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button("✕ Close Inspector")) {
                    device_tab_open_ = false;
                    focus_devices_list_ = true;
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine();
                ImGui::TextDisabled("| Inspected Hardware Address: %s", selected_device_->device_key.c_str());

                ImGui::Separator();

                // Full-width Device Details View
                DeviceDetailsView::render(selected_device_);

                ImGui::EndTabItem();
            }
        }

        // Tab 3: Full-Width RSSI Proximity Cluster Map
        if (ImGui::BeginTabItem("🗺️ RSSI Cluster Map")) {
            bool clicked = ClusterMapView::render(selected_device_);
            if (clicked && selected_device_) {
                device_tab_open_ = true;
                focus_device_tab_ = true;
            }
            ImGui::EndTabItem();
        }

        // Tab 4: Full-Width RF Spectrum & Waterfall
        if (ImGui::BeginTabItem("📊 RF Spectrum & Waterfall")) {
            SpectrumView::render();
            ImGui::EndTabItem();
        }

        // Tab 5: Full-Width Live Packet Stream & IDS Anomaly Alerts
        if (ImGui::BeginTabItem("📜 Packet Stream & Security IDS")) {
            PacketLogView::render(selected_device_);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

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
