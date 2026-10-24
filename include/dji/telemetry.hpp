// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

namespace dji {
// Timestamps use the ESP monotonic clock (esp_timer_get_time), in microseconds.
struct Armed { bool value; std::int64_t sampled_at_us; };
struct BatteryVoltage { float volts; std::int64_t sampled_at_us; };
struct BatteryCurrent { float amps; std::int64_t sampled_at_us; }; // signed
enum class BatteryCondition : std::uint8_t {
    Ok = 0, Warning = 1, Critical = 2, NotPresent = 3, Initializing = 4
};
// Remaining fields required by the native battery packet. Supply actual values;
// no cell-count estimation, charge integration, or alarm policy lives here.
// capacity_mah=0 explicitly means capacity is not configured.
struct BatteryDetails {
    std::uint8_t cell_count;
    std::uint16_t capacity_mah;
    std::uint16_t consumed_mah;
    BatteryCondition condition;
    std::int64_t sampled_at_us;
};
enum class Signal { Armed, BatteryVoltage, BatteryCurrent, BatteryDetails };
} // namespace dji
