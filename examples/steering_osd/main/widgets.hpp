// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "dji/frame.hpp"
#include <optional>

// These are example/application widgets, not part of the component API.
namespace playground {
struct Region { std::uint8_t x = 1, y = 3, width = 28, height = 13; };
struct WidgetConfig {
    bool canvas_test_pattern = false;
    Region turning_region{};
    bool center_turning_region = true;
    float horizontal_cells_per_m = 6, cell_height_over_width = 1.5f;
    float lookahead_m = 2.5f, forward_tilt_deg = 0, perspective_distance_m = 4;
    bool show_speed = true, show_voltage = true, show_current = true;
    bool valid() const;
};
struct Scene {
    std::optional<float> speed_mps, pack_volts, current_amps, curvature_per_m;
};
// Application owns data validity/freshness. nullopt draws the example's unknown
// marker. Draws into a local frame; the caller decides when to commit it.
void draw_widgets(dji::Frame& frame, const WidgetConfig& config, const Scene& scene);
} // namespace playground
