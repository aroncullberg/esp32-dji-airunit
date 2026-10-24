// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

namespace dji {
struct OsdConfig {
    bool enabled = true;
    // Explicit fallback only; a valid peer announcement replaces these dimensions.
    std::uint8_t columns = 53, rows = 20;
    std::uint32_t refresh_ms = 100;
    std::uint32_t full_repaint_ms = 3000;
};
struct Config {
    // Required. Select a UART with no console or other client using it.
    int uart = -1, tx_pin = -1, rx_pin = -1;
    OsdConfig osd{};
    // Native MSP_BATTERY_STATE: requires fresh voltage, current, and details.
    bool native_battery = false;
    std::uint32_t armed_ttl_ms = 500;
    std::uint32_t telemetry_ttl_ms = 1000;
    bool valid() const; // portable checks; driver additionally checks target pins
};
} // namespace dji
