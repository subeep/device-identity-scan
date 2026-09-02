#pragma once

#include "core/device.hpp"
#include <string>

struct GLFWwindow;

namespace discan {

class UiManager {
public:
    UiManager();
    ~UiManager();

    bool initialize(const std::string& window_title = "KISMET++ Multi-Protocol SDR Wireless Sniffer & Device Tracker", int width = 1440, int height = 900);
    void run();
    void shutdown();

private:
    GLFWwindow* window_ = nullptr;
    DevicePtr selected_device_;

    void render_frame();
};

} // namespace discan
