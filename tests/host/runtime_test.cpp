// SPDX-License-Identifier: GPL-3.0-only
#include "dji/air_unit.hpp"
#include "idf_uart_transport.hpp"
#include "msp_codec.hpp"
#include "esp_timer.h"
#include "freertos/task.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

namespace fake {
std::atomic<bool> owned{false}, fail_start{false}, fail_stop{false};
std::atomic<bool> hold_read{false}, in_read{false};
std::atomic<unsigned> draws{0};
}
// Replace only the transport boundary. Tests run the real AirUnit worker/core.
namespace dji::detail {
esp_err_t UartTransport::start(const Config& config) {
    if (fake::fail_start) return ESP_FAIL;
    if (fake::owned.exchange(true)) return ESP_ERR_INVALID_STATE;
    config_ = config; claimed_ = installed_ = true; return ESP_OK;
}
esp_err_t UartTransport::stop() {
    if (!claimed_) return ESP_OK;
    if (fake::fail_stop) return ESP_FAIL;
    fake::owned = false; claimed_ = installed_ = false; return ESP_OK;
}
int UartTransport::read(std::uint8_t*, std::size_t, bool& discontinuity) {
    fake::in_read = true;
    while (fake::hold_read) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    fake::in_read = false; discontinuity = false; return 0;
}
bool UartTransport::ready() const { (void)events_; return installed_; }
bool UartTransport::write(const std::uint8_t* bytes, std::size_t size) {
    MspParser parser; MspFrame frame;
    for (std::size_t i = 0; i < size; ++i)
        if (parser.feed(bytes[i], 0, frame) && frame.command == 182 && frame.size == 1 && frame.payload[0] == 4)
            ++fake::draws;
    return installed_;
}
}
namespace {
template<class Predicate> void await(Predicate predicate) {
    for (int i = 0; i < 500; ++i) {
        if (predicate()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "worker did not reach expected state");
}
}
int main() {
    dji::Config config; config.uart = 0; config.tx_pin = 4; config.rx_pin = 5;
    dji::AirUnit unit;
    assert(unit.update(dji::BatteryVoltage{1, esp_timer_get_time()}) == ESP_ERR_INVALID_STATE);
    assert(unit.start({}) == ESP_ERR_INVALID_ARG);
    fake::fail_start = true; assert(unit.start(config) == ESP_FAIL); fake::fail_start = false;
    assert(!unit.status().uart_owned);
    fake::fail_task = true; assert(unit.start(config) == ESP_ERR_NO_MEM); fake::fail_task = false;
    assert(!fake::owned && !unit.status().uart_owned);
    assert(unit.start(config) == ESP_OK);
    assert(unit.start(config) == ESP_ERR_INVALID_STATE);
    assert(unit.set_armed({true, esp_timer_get_time()}) == ESP_OK);
    auto frame = unit.osd().begin_frame();
    assert(frame.valid() && frame.columns() == 53 && frame.rows() == 20);
    assert(frame.text(2, 3, "GPS: example"));
    await([&] { return unit.status().tx_bytes > 0; });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    assert(fake::draws == 0); // drawing alone is not a submission
    assert(frame.commit() == ESP_OK);
    await([] { return fake::draws.load() > 0; });
    assert(unit.update(dji::BatteryDetails{6, 5000, 123, dji::BatteryCondition::Ok, esp_timer_get_time()}) == ESP_OK);
    await([&] { return unit.status().reported_armed; });
    assert(unit.status().uart_owned && unit.status().running);
    assert(unit.update(dji::BatteryVoltage{-1, esp_timer_get_time()}) == ESP_ERR_INVALID_ARG);
    assert(unit.status().rejected_updates == 1);
    // Force an unacknowledged worker exit; timeout must retain UART ownership.
    fake::hold_read = true;
    await([] { return fake::in_read.load(); });
    assert(unit.stop(0) == ESP_ERR_TIMEOUT);
    assert(unit.status().uart_owned && fake::owned);
    assert(unit.update(dji::BatteryVoltage{1, esp_timer_get_time()}) == ESP_ERR_INVALID_STATE);
    {
        dji::AirUnit second;
        assert(second.start(config) == ESP_ERR_INVALID_STATE);
    }
    fake::hold_read = false;
    assert(unit.stop(1000) == ESP_OK);
    assert(!fake::owned && !unit.status().uart_owned && !unit.status().running);
    assert(frame.commit() == ESP_ERR_INVALID_STATE);
    // Restart resets samples and counters, rather than replaying an old arm.
    assert(unit.start(config) == ESP_OK);
    assert(frame.commit() == ESP_ERR_INVALID_STATE); // previous session
    await([&] { return unit.status().tx_bytes > 0; });
    assert(!unit.status().reported_armed && unit.status().rejected_updates == 0);
    std::thread producer([&] {
        for (int i = 0; i < 1000; ++i)
            assert(unit.update(dji::BatteryVoltage{float(i % 100), esp_timer_get_time()}) == ESP_OK);
    });
    producer.join();
    fake::fail_stop = true;
    assert(unit.stop() == ESP_FAIL);
    assert(unit.status().uart_owned && !unit.status().running);
    assert(unit.start(config) == ESP_ERR_INVALID_STATE);
    fake::fail_stop = false;
    assert(unit.stop() == ESP_OK && !fake::owned);
    assert(unit.stop() == ESP_OK);
    std::puts("PASS: worker startup/rollback, concurrent updates, stop timeout/ownership, cleanup retry, restart");
}
