// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "msp_codec.hpp"
#include "state_store.hpp"
#include <cmath>

namespace dji::detail {
// Compact DJI status profile observed in the ELRS reference. Explicit wire
// serialization; no ABI-dependent structs and no synthetic sensor/GPS data.
inline bool append_status(TxBatch& batch, std::uint8_t command, bool armed) {
    std::uint8_t payload[22]{};
    payload[6] = armed ? 1 : 0; // uint32 flight-mode flags, bit 0 = armed
    payload[16] = 1;           // arming-disable flag count
    payload[17] = armed ? 0 : 1;
    return batch.frame('>', command, payload, sizeof(payload));
}
inline bool native_battery_ready(const StateStore& state, std::int64_t now, std::uint32_t ttl_ms) {
    // The legacy voltage field is 0.1 V in one byte. Avoid sending an incorrect
    // saturated voltage on peers which ignore the extended centivolt field.
    return state.voltage.fresh(now, ttl_ms) && state.current.fresh(now, ttl_ms) &&
        state.battery_details.fresh(now, ttl_ms) && state.voltage.value <= 25.5f &&
        state.current.value >= -327.68f && state.current.value <= 327.67f;
}
inline bool append_battery(TxBatch& batch, const StateStore& state) {
    const auto& details = state.battery_details.value;
    std::uint8_t payload[11]{};
    const auto write_u16 = [&](unsigned offset, std::uint16_t value) {
        payload[offset] = static_cast<std::uint8_t>(value);
        payload[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    };
    payload[0] = details.cell_count;
    write_u16(1, details.capacity_mah);
    payload[3] = static_cast<std::uint8_t>(std::lround(double(state.voltage.value) * 10));
    write_u16(4, details.consumed_mah);
    write_u16(6, static_cast<std::uint16_t>(static_cast<std::int16_t>(
        std::lround(double(state.current.value) * 100))));
    payload[8] = static_cast<std::uint8_t>(details.condition);
    write_u16(9, static_cast<std::uint16_t>(std::lround(double(state.voltage.value) * 100)));
    return batch.frame('>', 130, payload, sizeof(payload));
}
} // namespace dji::detail
