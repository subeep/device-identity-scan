#pragma once

#include "core/device.hpp"

namespace discan {

class PacketLogView {
public:
    static void render(DevicePtr& selected_device);

private:
    static bool auto_scroll_;
    static bool show_alerts_only_;
};

} // namespace discan
