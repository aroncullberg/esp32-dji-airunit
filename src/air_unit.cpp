// SPDX-License-Identifier: GPL-3.0-only
#include "dji/air_unit.hpp"
#include "engine.hpp"
#include "idf_uart_transport.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <new>

namespace dji {
namespace {
TickType_t ticks(std::uint32_t ms) {
    return static_cast<TickType_t>(std::max<std::uint64_t>(1,
        (std::uint64_t(ms) * configTICK_RATE_HZ + 999) / 1000));
}
}
struct AirUnit::Impl {
    std::mutex mutex;
    detail::StateStore inputs;
    detail::Engine engine;
    detail::UartTransport transport;
    Status cached{};
    Frame submitted;
    std::uint64_t submitted_revision = 0, consumed_revision = 0, session = 0;
    bool osd_enabled = false;
    std::atomic<bool> stop_requested{false};
    SemaphoreHandle_t done = nullptr;
    bool worker = false, accepting = false;
    std::uint64_t rejected = 0;

    template<class T> esp_err_t put(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!accepting) return ESP_ERR_INVALID_STATE;
        if (!inputs.put(value, esp_timer_get_time())) { ++rejected; return ESP_ERR_INVALID_ARG; }
        return ESP_OK;
    }
    static void entry(void* pointer) { static_cast<Impl*>(pointer)->run(); }
    void run() {
        std::uint8_t rx[256];
        detail::TxBatch tx;
        while (!stop_requested.load()) {
            bool discontinuity = false;
            const int count = transport.read(rx, sizeof(rx), discontinuity);
            if (discontinuity || count < 0) engine.discontinuity();
            const auto now = esp_timer_get_time();
            if (count > 0) engine.feed(rx, std::size_t(count), now);
            detail::StateStore snapshot;
            {
                std::lock_guard<std::mutex> lock(mutex);
                snapshot = inputs;
                if (submitted_revision != consumed_revision) {
                    engine.submit(submitted);
                    consumed_revision = submitted_revision;
                }
            }
            if (transport.ready() && engine.prepare(snapshot, now, tx))
                engine.commit(transport.write(tx.bytes.data(), tx.size));
            {
                std::lock_guard<std::mutex> lock(mutex);
                cached = engine.status(); cached.running = true; cached.uart_owned = true;
            }
            vTaskDelay(ticks(10));
        }
        xSemaphoreGive(done);
        vTaskDelete(nullptr);
    }
};
AirUnit::AirUnit() noexcept : impl_(new (std::nothrow) Impl) {}
AirUnit::~AirUnit() {
    if (!impl_) return;
    // Never destroy storage still referenced by a worker after a timed-out stop.
    // On unrecoverable cleanup failure retain Impl/resources rather than reset
    // the vehicle or falsely release ownership. Applications should explicitly
    // stop/check/retry before destruction so this exceptional leak is avoided.
    if (stop(1000) != ESP_OK) return;
    if (impl_->done) vSemaphoreDelete(impl_->done);
    delete impl_;
}
esp_err_t AirUnit::start(const Config& config) {
    if (!impl_) return ESP_ERR_NO_MEM;
    if (impl_->worker || impl_->transport.owned()) return ESP_ERR_INVALID_STATE;
    if (!config.valid()) return ESP_ERR_INVALID_ARG;
    if (!impl_->done) impl_->done = xSemaphoreCreateBinary();
    if (!impl_->done) return ESP_ERR_NO_MEM;
    while (xSemaphoreTake(impl_->done, 0) == pdTRUE) {}
    const auto result = impl_->transport.start(config);
    if (result != ESP_OK) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->cached.last_error = result; impl_->cached.uart_owned = impl_->transport.owned();
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->inputs = {}; impl_->rejected = 0; impl_->engine.start(config);
        impl_->submitted_revision = impl_->consumed_revision = 0;
        ++impl_->session; impl_->osd_enabled = config.osd.enabled;
        impl_->cached = impl_->engine.status(); impl_->cached.uart_owned = true;
    }
    impl_->stop_requested.store(false);
    // Engine/grid storage is in Impl; the worker uses a modest snapshot/IO stack.
    if (xTaskCreate(Impl::entry, "dji_air", 4096, impl_, 3, nullptr) != pdPASS) {
        const auto cleanup = impl_->transport.stop();
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->cached.last_error = cleanup == ESP_OK ? ESP_ERR_NO_MEM : cleanup;
        impl_->cached.uart_owned = impl_->transport.owned();
        return impl_->cached.last_error;
    }
    impl_->worker = true;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->accepting = true; impl_->cached.running = true;
    }
    return ESP_OK;
}
esp_err_t AirUnit::stop(std::uint32_t timeout_ms) {
    if (!impl_) return ESP_OK;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->accepting = false;
    }
    impl_->stop_requested.store(true);
    if (impl_->worker) {
        if (xSemaphoreTake(impl_->done, timeout_ms ? ticks(timeout_ms) : 0) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        impl_->worker = false;
    }
    const auto result = impl_->transport.stop();
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->cached.running = false; impl_->cached.uart_owned = impl_->transport.owned();
    if (result != ESP_OK) impl_->cached.last_error = result;
    return result;
}
esp_err_t AirUnit::set_armed(Armed v) { return impl_ ? impl_->put(v) : ESP_ERR_NO_MEM; }
esp_err_t AirUnit::update(BatteryVoltage v) { return impl_ ? impl_->put(v) : ESP_ERR_NO_MEM; }
esp_err_t AirUnit::update(BatteryCurrent v) { return impl_ ? impl_->put(v) : ESP_ERR_NO_MEM; }
esp_err_t AirUnit::update(BatteryDetails v) { return impl_ ? impl_->put(v) : ESP_ERR_NO_MEM; }
esp_err_t AirUnit::invalidate(Signal signal) {
    if (!impl_) return ESP_ERR_NO_MEM;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->accepting) return ESP_ERR_INVALID_STATE;
    return impl_->inputs.invalidate(signal) ? ESP_OK : ESP_ERR_INVALID_ARG;
}
Status AirUnit::status() const {
    if (!impl_) { Status result; result.last_error = ESP_ERR_NO_MEM; return result; }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto result = impl_->cached; result.rejected_updates = impl_->rejected; return result;
}
Frame AirUnit::begin_frame() {
    if (!impl_) return Frame(0, 0);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->accepting || !impl_->osd_enabled) return Frame(0, 0);
    Frame frame(impl_->cached.columns, impl_->cached.rows);
    frame.owner_ = this; frame.session_ = impl_->session;
    return frame;
}
esp_err_t AirUnit::submit_frame(const Frame& frame) {
    if (!impl_) return ESP_ERR_NO_MEM;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->accepting || !impl_->osd_enabled || frame.owner_ != this || frame.session_ != impl_->session)
        return ESP_ERR_INVALID_STATE;
    if (frame.columns() != impl_->cached.columns || frame.rows() != impl_->cached.rows)
        return ESP_ERR_INVALID_ARG;
    impl_->submitted = frame;
    ++impl_->submitted_revision;
    return ESP_OK;
}
Frame Osd::begin_frame() const { return owner_.begin_frame(); }
esp_err_t Frame::commit() const {
    return owner_ ? owner_->submit_frame(*this) : ESP_ERR_INVALID_STATE;
}
} // namespace dji
