# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "DJI Air currently targets ESP32-S3")
endif()
idf_component_register(
    SRCS "src/air_unit.cpp" "src/config.cpp" "src/msp_codec.cpp"
         "src/canvas.cpp" "src/frame.cpp" "src/engine.cpp"
         "src/idf_uart_transport.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_common
    PRIV_REQUIRES esp_driver_uart esp_driver_gpio esp_timer freertos pthread
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
