#pragma once
#include "FreeRTOS.h"
#include <atomic>
#include <chrono>
#include <thread>
using TaskHandle_t = void*;
namespace fake { inline std::atomic<bool> fail_task{false}; }
inline BaseType_t xTaskCreate(void (*entry)(void*), const char*, unsigned, void* argument,
                             unsigned, TaskHandle_t*) {
    if (fake::fail_task.load()) return pdFALSE;
    std::thread(entry, argument).detach();
    return pdPASS;
}
inline void vTaskDelay(TickType_t ticks) { std::this_thread::sleep_for(std::chrono::milliseconds(ticks)); }
inline void vTaskDelete(TaskHandle_t) {}
