idf_component_register(
  SRCS
    "main.cpp"
    "board.cpp"
    "gyro_switch_pro.cpp"
    "mouse_gyro_model.cpp"
    "mouse_host.cpp"
    "max3421_port.cpp"
  INCLUDE_DIRS "."
  REQUIRES
    # espp: Switch Pro protocol engine, USB device wrapper, HID report structs
    switch_pro usb_device hid-rp logger task timer base_component
    # TinyUSB device wrapper + TinyUSB + our host-stack component
    esp_tinyusb tinyusb tinyusb_host
    # ESP-IDF drivers for the board GPIOs and the MAX3421E SPI link
    esp_driver_gpio esp_driver_spi esp_timer freertos pthread
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
