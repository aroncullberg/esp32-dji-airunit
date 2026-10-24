// SPDX-License-Identifier: GPL-3.0-only
#include "dji/air_unit.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

extern "C" void app_main() {
    dji::Config config;
    config.uart = CONFIG_DJI_EXAMPLE_UART;
    config.tx_pin = CONFIG_DJI_EXAMPLE_TX;
    config.rx_pin = CONFIG_DJI_EXAMPLE_RX;
    config.osd.enabled = false; // MSP status/replies only: no DisplayPort traffic.
    if (!config.valid()) {
        ESP_LOGW("bench", "Set the exclusive UART and GPIOs in menuconfig > DJI bench example");
        return;
    }
    static dji::AirUnit unit;
    ESP_ERROR_CHECK(unit.start(config));
    std::int64_t next_report = 0;
    while (true) {
        const auto now = esp_timer_get_time();
        // This bench has no vehicle state. Explicitly report disarmed.
        ESP_ERROR_CHECK(unit.set_armed({false, now}));
        if (now >= next_report) {
            const auto s = unit.status();
            ESP_LOGI("msp", "TX=%llu RX=%llu frames=%llu last_cmd=%u armed=%d drop_req=%llu parse_err=%llu io_err=%llu/%llu",
                     static_cast<unsigned long long>(s.tx_bytes),
                     static_cast<unsigned long long>(s.rx_bytes),
                     static_cast<unsigned long long>(s.rx_frames), unsigned(s.last_rx_command),
                     int(s.reported_armed), static_cast<unsigned long long>(s.dropped_requests),
                     static_cast<unsigned long long>(s.parser_errors),
                     static_cast<unsigned long long>(s.rx_errors),
                     static_cast<unsigned long long>(s.tx_errors));
            next_report = now + 1000000;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
