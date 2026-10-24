#pragma once
#include <cstdint>
using TickType_t = std::uint32_t;
using BaseType_t = int;
constexpr BaseType_t pdTRUE = 1, pdFALSE = 0, pdPASS = 1;
constexpr unsigned configTICK_RATE_HZ = 1000;
