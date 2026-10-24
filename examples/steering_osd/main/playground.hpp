// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "dji/config.hpp"
#include "widgets.hpp"
#include "sdkconfig.h"

// Edit these settings, rebuild, and compare the result in the goggles.
// All values are synthetic bench inputs, not vehicle measurements.
namespace playground {
inline constexpr bool sweep = true; // false holds fixed_curvature_per_m
inline constexpr float fixed_curvature_per_m = 0.0f; // + left, - right, 0 straight
inline constexpr float sweep_limit_per_m = 0.7f;
inline constexpr double sweep_period_s = 10.0;
inline constexpr float speed_mps = 3.0f;
inline constexpr float pack_volts = 24.0f;
inline constexpr float current_amps = 1.5f;
inline constexpr std::uint8_t battery_cells = 6;
inline constexpr std::uint16_t battery_capacity_mah = 5000, battery_consumed_mah = 123;
static_assert(sweep_period_s > 0, "Sweep period must be positive");

inline dji::OsdConfig osd_config() {
    dji::OsdConfig osd;
    osd.enabled = true;
    // menuconfig > DJI bench example; an announcement overrides the fallback.
    osd.columns = CONFIG_DJI_EXAMPLE_OSD_COLUMNS;
    osd.rows = CONFIG_DJI_EXAMPLE_OSD_ROWS;
    osd.refresh_ms = 100;
    osd.full_repaint_ms = 3000;
    return osd;
}
inline WidgetConfig widgets_config() {
    WidgetConfig view;
#ifdef CONFIG_DJI_EXAMPLE_CANVAS_TEST
    view.canvas_test_pattern = true;
#endif
    view.turning_region = {1, 3, 28, 13}; // x, y, width, height
    view.center_turning_region = true; // follows the active canvas, not the fallback width
    view.horizontal_cells_per_m = 6.0f;
    view.cell_height_over_width = 1.5f;
    view.lookahead_m = 2.5f;
    view.forward_tilt_deg = 50.0f; // 0 top-down; increase to lay the path forward
    view.perspective_distance_m = 4.0f; // smaller = stronger distance taper
    view.show_speed = true;
    view.show_voltage = true;
    view.show_current = true;
    return view;
}
} // namespace playground
