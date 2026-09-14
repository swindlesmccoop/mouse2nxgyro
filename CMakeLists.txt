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

# Only the espp components we use (not the whole components/ tree), plus the
# vendored TinyUSB device wrapper and TinyUSB itself, plus our own components.
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
  "${CMAKE_CURRENT_LIST_DIR}/components"
)

# TinyUSB HOST stack configuration.
#
# esp_tinyusb owns tusb_config.h and only configures the DEVICE role (CFG_TUD_*);
# it never mentions CFG_TUH_*. TinyUSB's tusb_option.h fills in any option that
# is not already defined, so the host role is enabled here as GLOBAL compile
# definitions: they must be identical in every translation unit that includes
# tusb_option.h (the tinyusb component's tusb.c, our tinyusb_host component, and
# main), which per-component definitions could not guarantee.
#
# Host controller = MAX3421E over SPI (CFG_TUH_MAX3421). The ESP32-S3's single
# native USB-OTG controller stays in device mode for the Switch.
set(M2G_TUH_DEFINES
  CFG_TUH_ENABLED=1
  CFG_TUH_MAX3421=1
  CFG_TUH_MAX_SPEED=OPT_MODE_FULL_SPEED
  CFG_TUH_HUB=0
  CFG_TUH_DEVICE_MAX=1
  CFG_TUH_ENUMERATION_BUFSIZE=256
  CFG_TUH_HID=2
  CFG_TUH_HID_EPIN_BUFSIZE=64
  CFG_TUH_HID_EPOUT_BUFSIZE=64
)
# idf_build_set_property(<prop> <value> APPEND) appends exactly ONE value per
# call; passing several would silently keep only the first.
foreach(def IN LISTS M2G_TUH_DEFINES)
  idf_build_set_property(COMPILE_OPTIONS "-D${def}" APPEND)
endforeach()

set(COMPONENTS "main esptool_py" CACHE STRING "Top-level components; dependencies are pulled in via REQUIRES")

project(mouse2nxgyro)
