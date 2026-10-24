# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH "Project configuration")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(dji_steering_osd)
