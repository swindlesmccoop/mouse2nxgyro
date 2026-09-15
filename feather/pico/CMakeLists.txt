cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH is not set")
endif()

# Must be set before pico_sdk_import.cmake (it caches PICO_BOARD=pico otherwise).
set(PICO_BOARD adafruit_feather_rp2040_usb_host)

include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(mouse_relay C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

if(NOT DEFINED ENV{PICO_PIO_USB_PATH})
  message(FATAL_ERROR "PICO_PIO_USB_PATH is not set")
endif()
add_subdirectory("$ENV{PICO_PIO_USB_PATH}" pico_pio_usb)

add_executable(mouse_relay
  main.c
  usb_descriptors.c
  ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

target_compile_definitions(mouse_relay PRIVATE PIO_USB_USE_TINYUSB)
target_include_directories(mouse_relay PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(mouse_relay PRIVATE -Wall -Wextra)
target_link_libraries(mouse_relay PRIVATE
  pico_stdlib
  pico_multicore
  pico_pio_usb
  tinyusb_device
  tinyusb_host
  hardware_clocks
  hardware_gpio
)

# TinyUSB owns native USB-C; do not also enable pico stdio USB.
pico_enable_stdio_usb(mouse_relay 0)
pico_enable_stdio_uart(mouse_relay 0)
pico_add_extra_outputs(mouse_relay)
