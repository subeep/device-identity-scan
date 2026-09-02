#pragma once

#include "core/device.hpp"
#include <string>

struct GLFWwindow;

namespace discan {

class UiManager {
public:
    UiManager();
    ~UiManager();

    bool initialize(const std::string& window_title = "Device Identity Scan Engine - Multi-Protocol SDR Sniffer", int width = 1500, int height = 920);
    void run();
    void shutdown();

private:
    GLFWwindow* window_ = nullptr;
    DevicePtr selected_device_;
    bool device_tab_open_ = false;
    bool focus_device_tab_ = false;
    bool focus_devices_list_ = false;

    void render_frame();
};

} // namespace discan
