// SPDX-License-Identifier: GPL-3.0-only
#include "widgets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace playground {
bool WidgetConfig::valid() const {
    const auto& r = turning_region;
    return r.x < dji::Frame::max_columns && r.y >= 3 && r.y < dji::Frame::max_rows &&
        r.width >= 3 && r.width <= dji::Frame::max_columns &&
        r.height >= 2 && r.height <= dji::Frame::max_rows &&
        std::isfinite(horizontal_cells_per_m) && horizontal_cells_per_m >= 0.1f && horizontal_cells_per_m <= 100 &&
        std::isfinite(cell_height_over_width) && cell_height_over_width >= 0.25f && cell_height_over_width <= 4 &&
        std::isfinite(lookahead_m) && lookahead_m >= 0.1f && lookahead_m <= 100 &&
        std::isfinite(forward_tilt_deg) && forward_tilt_deg >= 0 && forward_tilt_deg <= 80 &&
        std::isfinite(perspective_distance_m) && perspective_distance_m >= 0.1f && perspective_distance_m <= 100;
}
void draw_widgets(dji::Frame& frame, const WidgetConfig& c, const Scene& v) {
    frame.clear();
    if (!frame.valid() || !c.valid()) return;
    const auto columns = frame.columns(), rows = frame.rows();
    const auto put = [&](int x, int y, char value) {
        frame.glyph(x, y, dji::Glyph{static_cast<std::uint8_t>(value), false});
    };
    char text[61];
    if (c.canvas_test_pattern) {
        // Coordinates, not resolution negotiation: reveal the visible bounds
        // of the configured canvas even when the peer never announces its size.
        for (unsigned x = 0; x < frame.columns(); ++x) {
            put(x, 0, '-'); put(x, frame.rows() - 1, '-');
        }
        for (unsigned y = 0; y < frame.rows(); ++y) {
            put(0, y, '|'); put(frame.columns() - 1, y, '|');
        }
        put(0, 0, '+'); put(frame.columns() - 1, 0, '+');
        put(0, frame.rows() - 1, '+'); put(frame.columns() - 1, frame.rows() - 1, '+');
        if (frame.columns() > 2 && frame.rows() > 2) {
            std::snprintf(text, sizeof(text), "CANVAS %ux%u", frame.columns(), frame.rows());
            if (std::strlen(text) > frame.columns() - 2)
                std::snprintf(text, sizeof(text), "%ux%u", frame.columns(), frame.rows());
            if (std::strlen(text) <= frame.columns() - 2)
                frame.text(int((frame.columns() - std::strlen(text)) / 2), 1, text);
        }
        put(frame.columns() / 2, frame.rows() / 2, '+');
        return;
    }
    if (c.show_speed) {
        if (v.speed_mps && std::isfinite(*v.speed_mps) && *v.speed_mps >= 0)
            std::snprintf(text, sizeof(text), "SPD %.1f km/h", double(*v.speed_mps) * 3.6);
        else std::snprintf(text, sizeof(text), "SPD -- km/h");
        frame.text(0, 0, text);
    }
    if (c.show_voltage) {
        if (v.pack_volts && std::isfinite(*v.pack_volts) && *v.pack_volts >= 0)
            std::snprintf(text, sizeof(text), "PACK %.2f V", double(*v.pack_volts));
        else std::snprintf(text, sizeof(text), "PACK -- V");
        frame.text(0, 1, text);
    }
    if (c.show_current) {
        if (v.current_amps && std::isfinite(*v.current_amps))
            std::snprintf(text, sizeof(text), "CUR %.2f A", double(*v.current_amps));
        else std::snprintf(text, sizeof(text), "CUR -- A");
        frame.text(0, 2, text);
    }
    const auto& r = c.turning_region;
    const int region_width = std::min<unsigned>(r.width, columns);
    const int region_left = c.center_turning_region ? (int(columns) - region_width) / 2 : r.x;
    const int right = std::min<unsigned>(unsigned(region_left) + region_width, columns);
    const int bottom = std::min<unsigned>(unsigned(r.y) + r.height, rows);
    if (right <= region_left || bottom <= r.y) return;
    const int origin_x = region_left + (right - region_left) / 2, origin_y = bottom - 1;
    if (v.curvature_per_m && std::isfinite(*v.curvature_per_m) && std::abs(*v.curvature_per_m) <= 100) {
        const double k = *v.curvature_per_m;
        const double tilt = c.forward_tilt_deg * 0.017453292519943295;
        const double sine = std::sin(tilt), cosine = std::cos(tilt);
        const double distance = c.perspective_distance_m;
        for (unsigned i = 0; i <= 256; ++i) {
            const double s = double(c.lookahead_m) * i / 256;
            const double a = k * s;
            const double forward = std::abs(a) < 1e-6 ? s : std::sin(a) / k;
            const double half_sin = std::sin(a / 2);
            const double left = std::abs(a) < 1e-6 ? k * s * s / 2 : 2 * half_sin * half_sin / k;
            // Pitch the ground plane, then divide by depth. Nearby geometry
            // retains its lateral scale; distant geometry narrows/foreshortens.
            // Reject points behind the virtual camera's near plane before any
            // division/rounding (tight curves may wrap behind the reference).
            const double depth = distance + forward * sine;
            if (depth < 0.05) continue;
            const double perspective = distance / depth;
            const int x = origin_x - int(std::lround(left * c.horizontal_cells_per_m * perspective));
            const int y = origin_y - int(std::lround(forward * cosine * c.horizontal_cells_per_m *
                                                    perspective / c.cell_height_over_width));
            if (x >= region_left && x < right && y >= r.y && y < bottom) put(x, y, '.');
        }
    } else if (origin_y > r.y) put(origin_x, origin_y - 1, '?');
    put(origin_x, origin_y, '^');
}
} // namespace playground
