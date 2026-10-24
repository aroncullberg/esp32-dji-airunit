// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cmath>
#include <cstdint>
#include "dji/telemetry.hpp"

namespace dji::detail {
template<class T> struct Sample {
    T value{};
    std::int64_t sampled_at_us = -1;
    bool valid = false;
    bool fresh(std::int64_t now, std::uint32_t ttl_ms) const {
        return valid && sampled_at_us >= 0 && now >= sampled_at_us &&
            now - sampled_at_us < std::int64_t(ttl_ms) * 1000;
    }
    bool put(T next, std::int64_t timestamp, std::int64_t now) {
        if (timestamp < 0 || timestamp > now || timestamp < sampled_at_us) return false;
        value = next;
        sampled_at_us = timestamp;
        valid = true;
        return true;
    }
};
struct StateStore {
    Sample<bool> armed;
    Sample<float> voltage, current;
    Sample<BatteryDetails> battery_details;
    bool put(Armed v, std::int64_t now) { return armed.put(v.value, v.sampled_at_us, now); }
    bool put(BatteryVoltage v, std::int64_t now) {
        return std::isfinite(v.volts) && v.volts >= 0 && v.volts <= 1000 &&
            voltage.put(v.volts, v.sampled_at_us, now);
    }
    bool put(BatteryCurrent v, std::int64_t now) {
        return std::isfinite(v.amps) && std::abs(v.amps) <= 10000 &&
            current.put(v.amps, v.sampled_at_us, now);
    }
    bool put(BatteryDetails v, std::int64_t now) {
        return v.cell_count != 0 &&
            static_cast<unsigned>(v.condition) <= static_cast<unsigned>(BatteryCondition::Initializing) &&
            battery_details.put(v, v.sampled_at_us, now);
    }
    bool invalidate(Signal signal) {
        switch (signal) {
        case Signal::Armed: armed.valid = false; break;
        case Signal::BatteryVoltage: voltage.valid = false; break;
        case Signal::BatteryCurrent: current.valid = false; break;
        case Signal::BatteryDetails: battery_details.valid = false; break;
        default: return false;
        }
        return true;
    }
};
} // namespace dji::detail
