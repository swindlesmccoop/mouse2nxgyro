cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH is not set")
endif()

set(PICO_BOARD adafruit_feather_rp2040_usb_host)

include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(mouse_relay C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

if(NOT DEFINED ENV{PICO_PIO_USB_PATH})
  message(FATAL_ERROR "PICO_PIO_USB_PATH is not set")
endif()
add_subdirectory("$ENV{PICO_PIO_USB_PATH}" pico_pio_usb)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

add_executable(mouse_relay
  main.cpp
  usb_descriptors.c
  switch_pro_pico.cpp
  binds.cpp
  ${REPO_ROOT}/main/mouse_gyro_model.cpp
  ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

target_compile_definitions(mouse_relay PRIVATE PIO_USB_USE_TINYUSB)
target_include_directories(mouse_relay PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${REPO_ROOT}/main
  ${REPO_ROOT}/external/espp/components/switch_pro/include
)
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

pico_enable_stdio_usb(mouse_relay 0)
pico_enable_stdio_uart(mouse_relay 0)
pico_add_extra_outputs(mouse_relay)
