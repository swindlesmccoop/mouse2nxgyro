# mouse2nxgyro -- ESP-IDF project.
#
# Reproducibility: the IDF Component Manager is DISABLED. Every dependency comes
# from the pinned git submodule tree under external/espp (espp components plus its
# vendored esp_tinyusb + tinyusb), so a build never reaches the component
# registry and a fresh clone builds exactly what the submodule commits say.
cmake_minimum_required(VERSION 3.20)

set(ENV{IDF_COMPONENT_MANAGER} "0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(ESPP_DIR "${CMAKE_CURRENT_LIST_DIR}/external/espp")

if(NOT EXISTS "${ESPP_DIR}/components/switch_pro/CMakeLists.txt"
   OR NOT EXISTS "${ESPP_DIR}/external/tinyusb/src/tusb.h"
   OR NOT EXISTS "${ESPP_DIR}/components/hid-rp/detail/hid-rp/hid-rp/hid/rdf/descriptor.hpp"
   OR NOT EXISTS "${ESPP_DIR}/components/format/detail/fmt/include/fmt/format.h")
  message(FATAL_ERROR
    "Submodules missing. Run ./scripts/bootstrap.sh (or ./scripts/dev.sh bootstrap) first.")
endif()

# Only the espp components we use, plus the vendored TinyUSB device wrapper.
set(EXTRA_COMPONENT_DIRS
  "${ESPP_DIR}/components/base_component"
  "${ESPP_DIR}/components/format"
  "${ESPP_DIR}/components/logger"
  "${ESPP_DIR}/components/task"
  "${ESPP_DIR}/components/timer"
  "${ESPP_DIR}/components/hid-rp"
  "${ESPP_DIR}/components/usb_device"
  "${ESPP_DIR}/components/switch_pro"
  "${ESPP_DIR}/external/esp-usb/device/esp_tinyusb"
  "${ESPP_DIR}/external/tinyusb"
)

# `relay` branch: mouse arrives as UART lines from a PC forwarder. TinyUSB is
# DEVICE-only (Pro Controller). Do not enable CFG_TUH_* here — that would pull
# in a host stack with no controller.

set(COMPONENTS "main esptool_py" CACHE STRING "Top-level components; dependencies are pulled in via REQUIRES")

project(mouse2nxgyro)
