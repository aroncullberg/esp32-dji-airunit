// SPDX-License-Identifier: GPL-3.0-only
#include "dji/config.hpp"
#include "dji/frame.hpp"

namespace dji {
bool Config::valid() const {
    const auto& o = osd;
    return uart >= 0 && uart < 3 && tx_pin >= 0 && rx_pin >= 0 && tx_pin != rx_pin &&
        o.columns >= 1 && o.columns <= Frame::max_columns &&
        o.rows >= 1 && o.rows <= Frame::max_rows &&
        o.refresh_ms >= 50 && o.refresh_ms <= 1000 && o.full_repaint_ms >= 1000 &&
        o.full_repaint_ms <= 60000 && armed_ttl_ms >= 100 && armed_ttl_ms <= 60000 &&
        telemetry_ttl_ms >= 100 && telemetry_ttl_ms <= 60000;
}
} // namespace dji
