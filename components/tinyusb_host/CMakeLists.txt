# TinyUSB HOST stack as an ESP-IDF component.
#
# The `tinyusb` IDF component (espressif/tinyusb, vendored by espp) compiles only
# the DEVICE half of TinyUSB: usbd.c, the *_device.c class drivers and the DWC2
# device controller driver. Nothing in it builds usbh.c, the HID host class
# driver, or the MAX3421E host controller driver. This component compiles exactly
# those files, from the SAME pinned TinyUSB tree, against the SAME tusb_config.h
# (esp_tinyusb's) and the same global CFG_TUH_* definitions set in the top-level
# CMakeLists.txt. No TinyUSB source is copied into this repo.
#
# The application still has to provide the three MAX3421E board callbacks
# (tuh_max3421_spi_xfer_api / tuh_max3421_spi_cs_api / tuh_max3421_int_api) --
# see main/max3421_port.cpp -- and run tuh_task() from a task.

idf_component_get_property(tinyusb_dir tinyusb COMPONENT_DIR)
idf_component_get_property(esp_tinyusb_dir esp_tinyusb COMPONENT_DIR)

if(NOT EXISTS "${tinyusb_dir}/src/host/usbh.c")
  message(FATAL_ERROR "tinyusb host sources not found under ${tinyusb_dir}")
endif()

set(srcs
  "${tinyusb_dir}/src/host/usbh.c"
  "${tinyusb_dir}/src/host/hub.c"                            # compiled out unless CFG_TUH_HUB
  "${tinyusb_dir}/src/class/hid/hid_host.c"
  "${tinyusb_dir}/src/portable/analog/max3421/hcd_max3421.c"
)

idf_component_register(
  SRCS ${srcs}
  # tusb_option.h / tusb.h come from the tinyusb component's public include dir
  # (src/). CFG_TUSB_MCU is a PUBLIC compile option of that component, so we
  # inherit it by REQUIRING it.
  REQUIRES tinyusb esp_tinyusb freertos
)

# tusb_option.h does `#include "tusb_config.h"`, which lives in esp_tinyusb's
# public include/ (so REQUIRES esp_tinyusb already provides it). Spelled out here
# anyway so the dependency on that exact file is explicit, and so the TinyUSB
# sources resolve their own relative includes ("host/hcd.h", ...).
target_include_directories(${COMPONENT_LIB} PRIVATE
  "${esp_tinyusb_dir}/include"
  "${tinyusb_dir}/src"
)

# TinyUSB's host code is not warning-clean under IDF's default flags.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter -Wno-type-limits)
