// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include "dji/config.hpp"
#include "driver/uart.h"

namespace dji::detail {
class UartTransport {
public:
    esp_err_t start(const Config& config);
    esp_err_t stop(); // only after worker exit; not a UART handoff contract
    int read(std::uint8_t* buffer, std::size_t capacity, bool& discontinuity);
    bool ready() const;
    bool write(const std::uint8_t* buffer, std::size_t size);
    bool owned() const { return claimed_; }
private:
    Config config_{};
    QueueHandle_t events_ = nullptr;
    bool installed_ = false, claimed_ = false;
};
} // namespace dji::detail
