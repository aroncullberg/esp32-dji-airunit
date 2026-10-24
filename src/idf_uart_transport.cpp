// SPDX-License-Identifier: GPL-3.0-only
#include "idf_uart_transport.hpp"
#include "driver/gpio.h"
#include <array>
#include <mutex>

namespace dji::detail {
namespace {
std::mutex ownership;
std::array<bool, UART_NUM_MAX> ports{};
std::array<bool, GPIO_NUM_MAX> pins{};
}
esp_err_t UartTransport::start(const Config& config) {
    if (claimed_) return ESP_ERR_INVALID_STATE;
    if (!config.valid() || config.uart >= UART_NUM_MAX ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config.tx_pin) || !GPIO_IS_VALID_GPIO(config.rx_pin))
        return ESP_ERR_INVALID_ARG;
    const auto port = static_cast<uart_port_t>(config.uart);
    {
        std::lock_guard<std::mutex> guard(ownership);
        if (ports[config.uart] || pins[config.tx_pin] || pins[config.rx_pin] ||
            uart_is_driver_installed(port)) return ESP_ERR_INVALID_STATE;
        ports[config.uart] = pins[config.tx_pin] = pins[config.rx_pin] = true;
        claimed_ = true; config_ = config;
    }
    // Install before touching routes/config, so an existing driver's resources
    // are never reconfigured as a side effect of our failed start.
    auto result = uart_driver_install(port, 1024, 1024, 16, &events_, 0);
    installed_ = result == ESP_OK;
    if (result != ESP_OK) {
        std::lock_guard<std::mutex> guard(ownership);
        ports[config.uart] = pins[config.tx_pin] = pins[config.rx_pin] = false;
        claimed_ = false;
        return result;
    }
    uart_config_t settings{};
    settings.baud_rate = 115200; settings.data_bits = UART_DATA_8_BITS;
    settings.parity = UART_PARITY_DISABLE; settings.stop_bits = UART_STOP_BITS_1;
    settings.flow_ctrl = UART_HW_FLOWCTRL_DISABLE; settings.source_clk = UART_SCLK_DEFAULT;
    result = uart_param_config(port, &settings);
    if (result == ESP_OK) result = uart_set_line_inverse(port, 0);
    if (result == ESP_OK) result = uart_set_pin(port, config.tx_pin, config.rx_pin,
                                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (result != ESP_OK) {
        const auto cleanup = stop();
        return cleanup == ESP_OK ? result : cleanup;
    }
    return ESP_OK;
}
esp_err_t UartTransport::stop() {
    if (!claimed_) return ESP_OK;
    if (installed_) {
        // Worker is gone. This is ordinary shutdown, without borrowing promises.
        uart_wait_tx_done(static_cast<uart_port_t>(config_.uart), pdMS_TO_TICKS(100));
        const auto result = uart_driver_delete(static_cast<uart_port_t>(config_.uart));
        if (result != ESP_OK) return result;
        installed_ = false; events_ = nullptr;
    }
    const auto tx = gpio_reset_pin(static_cast<gpio_num_t>(config_.tx_pin));
    const auto rx = gpio_reset_pin(static_cast<gpio_num_t>(config_.rx_pin));
    if (tx != ESP_OK || rx != ESP_OK) return tx != ESP_OK ? tx : rx;
    std::lock_guard<std::mutex> guard(ownership);
    ports[config_.uart] = pins[config_.tx_pin] = pins[config_.rx_pin] = false;
    claimed_ = false;
    return ESP_OK;
}
int UartTransport::read(std::uint8_t* buffer, std::size_t capacity, bool& discontinuity) {
    discontinuity = false;
    uart_event_t event;
    // A flooding peer cannot keep the worker in this loop indefinitely.
    for (unsigned i = 0; i < 16 && xQueueReceive(events_, &event, 0) == pdTRUE; ++i) {
        if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
            event.type == UART_PARITY_ERR || event.type == UART_FRAME_ERR || event.type == UART_BREAK)
            discontinuity = true;
    }
    if (discontinuity) {
        uart_flush_input(static_cast<uart_port_t>(config_.uart));
        xQueueReset(events_);
    }
    return uart_read_bytes(static_cast<uart_port_t>(config_.uart), buffer, capacity, 0);
}
bool UartTransport::ready() const {
    return installed_ && uart_wait_tx_done(static_cast<uart_port_t>(config_.uart), 0) == ESP_OK;
}
bool UartTransport::write(const std::uint8_t* buffer, std::size_t size) {
    // Called only with an empty TX ring/FIFO, by the sole writer. The <=384 byte
    // batch fits the 1024-byte TX ring; no control task ever waits on UART.
    return size <= 384 && uart_write_bytes(static_cast<uart_port_t>(config_.uart), buffer, size) == int(size);
}
} // namespace dji::detail
