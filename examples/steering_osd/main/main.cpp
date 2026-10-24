// SPDX-License-Identifier: GPL-3.0-only
#include "dji/air_unit.hpp"
#include "playground.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <cmath>

extern "C" void app_main() {
    dji::Config config;
    config.uart = CONFIG_DJI_EXAMPLE_UART;
    config.tx_pin = CONFIG_DJI_EXAMPLE_TX;
    config.rx_pin = CONFIG_DJI_EXAMPLE_RX;
    config.osd = playground::osd_config();
    const auto view = playground::widgets_config();
#ifdef CONFIG_DJI_EXAMPLE_NATIVE_BATTERY
    config.native_battery = true;
#endif
    if (!config.valid() || !view.valid()) {
        ESP_LOGW("bench", "Set the exclusive UART and GPIOs in menuconfig > DJI bench example");
        return;
    }
    static dji::AirUnit unit;
    ESP_ERROR_CHECK(unit.start(config));
    std::int64_t next_report = 0;
    while (true) {
        const auto now = esp_timer_get_time();
        // Bench data only. No actual vehicle state is available in this example.
        // Remain disarmed: never synthesize an arm transition for DJI.
        ESP_ERROR_CHECK(unit.set_armed({false, now}));
        const double phase = std::fmod(double(now) / 1000000.0, playground::sweep_period_s) /
                             playground::sweep_period_s;
        const float curvature = playground::sweep
            ? playground::sweep_limit_per_m * float(std::sin(phase * 6.283185307179586))
            : playground::fixed_curvature_per_m;
        ESP_ERROR_CHECK(unit.update(dji::BatteryVoltage{playground::pack_volts, now}));
        ESP_ERROR_CHECK(unit.update(dji::BatteryCurrent{playground::current_amps, now}));
        ESP_ERROR_CHECK(unit.update(dji::BatteryDetails{
            playground::battery_cells, playground::battery_capacity_mah,
            playground::battery_consumed_mah, dji::BatteryCondition::Ok, now}));
        // The example owns every cell and all widget formatting/geometry.
        // Native telemetry above does not draw anything into this frame.
        auto frame = unit.osd().begin_frame();
        const playground::Scene scene{
            playground::speed_mps, playground::pack_volts, playground::current_amps, curvature};
        playground::draw_widgets(frame, view, scene);
        const auto result = frame.commit();
        // A live canvas-size change can invalidate a frame; rebuild next loop.
        if (result != ESP_OK && result != ESP_ERR_INVALID_ARG) ESP_ERROR_CHECK(result);
        if (now >= next_report) {
            const auto s = unit.status();
            ESP_LOGI("bench", "TX=%llu RX=%llu frames=%llu last_cmd=%u canvas=%ux%u announced=%d armed=%d battery_ready=%d battery_req=%llu battery_tx=%llu drop_req=%llu parse_err=%llu io_err=%llu/%llu",
                     static_cast<unsigned long long>(s.tx_bytes),
                     static_cast<unsigned long long>(s.rx_bytes),
                     static_cast<unsigned long long>(s.rx_frames), unsigned(s.last_rx_command),
                     unsigned(s.columns), unsigned(s.rows), int(s.canvas_announced),
                     int(s.reported_armed), int(s.native_battery_ready),
                     static_cast<unsigned long long>(s.battery_requests),
                     static_cast<unsigned long long>(s.battery_tx_frames),
                     static_cast<unsigned long long>(s.dropped_requests),
                     static_cast<unsigned long long>(s.parser_errors),
                     static_cast<unsigned long long>(s.rx_errors),
                     static_cast<unsigned long long>(s.tx_errors));
            next_report = now + 1000000;
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // producers faster than the 10 Hz OSD
    }
}
