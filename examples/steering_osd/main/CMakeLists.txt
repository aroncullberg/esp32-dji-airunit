# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(SRCS "main.cpp" "widgets.cpp" INCLUDE_DIRS "."
    REQUIRES esp32-dji-air esp_timer freertos)
