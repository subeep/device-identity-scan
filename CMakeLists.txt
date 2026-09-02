cmake_minimum_required(VERSION 3.16)
project(device-identity-scan LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warning flags
add_compile_options(-Wall -Wextra -O3)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Find OpenGL & GLFW
find_package(OpenGL REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

# Check for UHD
pkg_check_modules(UHD uhd)
if (UHD_FOUND)
    message(STATUS "UHD found: ${UHD_VERSION}")
    add_definitions(-DHAVE_UHD=1)
else()
    message(WARNING "UHD not found via pkg-config; UHD support might be disabled")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/implot
    ${OPENGL_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${UHD_INCLUDE_DIRS}
)

# Common Core Sources
set(CORE_SOURCES
    src/common/oui_database.cpp
    src/protocols/wifi_dissector.cpp
    src/protocols/ble_dissector.cpp
    src/protocols/zigbee_dissector.cpp
    src/protocols/lora_dissector.cpp
    src/core/device_tracker.cpp
    src/core/ids_engine.cpp
    src/core/packet_storage.cpp
    src/core/wifi_live_scanner.cpp
    src/dsp/fft_analyzer.cpp
    src/dsp/ble_demod.cpp
    src/dsp/zigbee_demod.cpp
    src/dsp/lora_demod.cpp
    src/dsp/wifi_demod.cpp
    src/sdr/hackrf_device.cpp
    src/sdr/usrp_device.cpp
    src/sdr/simulated_device.cpp
    src/sdr/sdr_manager.cpp
)

# UI Sources
set(UI_SOURCES
    src/ui/ui_manager.cpp
    src/ui/sdr_controls_view.cpp
    src/ui/device_table_view.cpp
    src/ui/device_details_view.cpp
    src/ui/spectrum_view.cpp
    src/ui/packet_log_view.cpp
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_impl_glfw.cpp
    vendor/imgui/imgui_impl_opengl3.cpp
    vendor/implot/implot.cpp
    vendor/implot/implot_items.cpp
)

# Main Executable: device_identity_scan
add_executable(device_identity_scan
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)

target_link_libraries(device_identity_scan PRIVATE
    Threads::Threads
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${UHD_LIBRARIES}
    ${CMAKE_DL_LIBS}
    m
)

# Backward Compatibility Target: sdr_wireless_tracker
add_executable(sdr_wireless_tracker
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)

target_link_libraries(sdr_wireless_tracker PRIVATE
    Threads::Threads
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${UHD_LIBRARIES}
    ${CMAKE_DL_LIBS}
    m
)

# Unit Tests Executable
add_executable(unit_tests
    test/unit_tests.cpp
    ${CORE_SOURCES}
)

target_link_libraries(unit_tests PRIVATE
    Threads::Threads
    ${UHD_LIBRARIES}
    ${CMAKE_DL_LIBS}
    m
)

# Physical HackRF Hardware Test Executable
add_executable(test_hackrf_hardware
    test/test_hackrf_hardware.cpp
    ${CORE_SOURCES}
)

target_link_libraries(test_hackrf_hardware PRIVATE
    Threads::Threads
    ${UHD_LIBRARIES}
    ${CMAKE_DL_LIBS}
    m
)
