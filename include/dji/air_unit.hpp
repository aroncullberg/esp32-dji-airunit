// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "dji/config.hpp"
#include "dji/osd.hpp"
#include "dji/status.hpp"
#include "dji/telemetry.hpp"

namespace dji {
// Task-context API. Serialize start/stop in the application. Setters/status may
// run concurrently; they copy state and never perform UART I/O.
class AirUnit {
public:
    AirUnit() noexcept;
    ~AirUnit();
    AirUnit(const AirUnit&) = delete;
    AirUnit& operator=(const AirUnit&) = delete;
    AirUnit(AirUnit&&) = delete;
    AirUnit& operator=(AirUnit&&) = delete;

    esp_err_t start(const Config& config);
    // Timeout covers worker acknowledgement; driver cleanup may add ~100 ms.
    // Timeout leaves ownership intact. Retry stop; do not reuse that UART.
    esp_err_t stop(std::uint32_t timeout_ms = 1000);
    esp_err_t set_armed(Armed value);
    esp_err_t update(BatteryVoltage value);
    esp_err_t update(BatteryCurrent value);
    esp_err_t update(BatteryDetails value);
    esp_err_t invalidate(Signal signal);
    Osd osd() { return Osd(*this); }
    Status status() const;
private:
    friend class Osd;
    friend class Frame;
    Frame begin_frame();
    esp_err_t submit_frame(const Frame& frame);
    struct Impl;
    Impl* impl_;
};
} // namespace dji
