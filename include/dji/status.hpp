// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include "esp_err.h"

namespace dji {
struct Status {
    bool running = false;
    bool uart_owned = false;
    bool canvas_announced = false; // not proof of a visible/working display
    bool reported_armed = false;
    bool native_battery_ready = false;
    std::uint8_t columns = 0, rows = 0, last_rx_command = 0;
    std::int64_t last_rx_us = 0;
    std::uint64_t rx_bytes = 0, tx_bytes = 0, rx_frames = 0;
    std::uint64_t parser_errors = 0, rx_errors = 0, tx_errors = 0;
    std::uint64_t dropped_requests = 0, rejected_updates = 0, display_frames = 0;
    std::uint64_t battery_requests = 0, battery_tx_frames = 0;
    esp_err_t last_error = ESP_OK;
};
} // namespace dji
