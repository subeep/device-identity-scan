#pragma once

#include "core/device.hpp"
#include <string>

namespace discan {

class DeviceTableView {
public:
    static void render(DevicePtr& selected_device);

private:
    static bool show_wifi_;
    static bool show_ble_;
    static bool show_zigbee_;
    static bool show_lora_;
    static char search_filter_[128];
};

} // namespace discan
