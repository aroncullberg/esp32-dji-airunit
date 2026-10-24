#pragma once
#include "FreeRTOS.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
struct FakeSemaphore {
    std::mutex mutex;
    std::condition_variable changed;
    bool ready = false;
};
using SemaphoreHandle_t = FakeSemaphore*;
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return new FakeSemaphore; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks) {
    std::unique_lock<std::mutex> guard(semaphore->mutex);
    if (!semaphore->changed.wait_for(guard, std::chrono::milliseconds(ticks),
                                    [&] { return semaphore->ready; })) return pdFALSE;
    semaphore->ready = false;
    return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t semaphore) {
    std::lock_guard<std::mutex> guard(semaphore->mutex);
    semaphore->ready = true;
    semaphore->changed.notify_one();
}
inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) { delete semaphore; }
