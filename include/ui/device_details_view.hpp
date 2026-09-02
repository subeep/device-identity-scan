#pragma once

#include "core/device.hpp"

namespace discan {

class DeviceDetailsView {
public:
    static void render(const DevicePtr& selected_device);

private:
    static void render_wifi_kismet_view(const DevicePtr& dev);
    static void render_dissection_tree(const std::vector<DissectedField>& tree);
    static void render_hex_view(const std::vector<uint8_t>& raw_data);
};

} // namespace discan
