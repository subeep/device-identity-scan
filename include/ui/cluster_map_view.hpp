#pragma once

#include "core/device.hpp"

namespace discan {

enum class ClusterMapMode {
    RADIAL_PROXIMITY = 0,
    SCATTER_2D,
    ZONE_BREAKDOWN
};

class ClusterMapView {
public:
    static bool render(DevicePtr& selected_device);

private:
    static ClusterMapMode current_mode_;
    static bool show_wifi_;
    static bool show_ble_;
    static bool show_zigbee_;
    static bool show_lora_;
    static float radar_sweep_angle_;
    static float immediate_threshold_; // -50 dBm
    static float near_threshold_;      // -65 dBm
    static float mid_threshold_;       // -80 dBm

    static void render_radial_proximity(const std::vector<DevicePtr>& devices, DevicePtr& selected_device, bool& device_clicked);
    static void render_scatter_2d(const std::vector<DevicePtr>& devices, DevicePtr& selected_device, bool& device_clicked);
    static void render_zone_breakdown(const std::vector<DevicePtr>& devices, DevicePtr& selected_device, bool& device_clicked);
};

} // namespace discan
