idf_component_register(
  SRCS
    "main.cpp"
    "board.cpp"
    "gyro_switch_pro.cpp"
    "mouse_gyro_model.cpp"
    "mouse_host.cpp"
  INCLUDE_DIRS "."
  REQUIRES
    switch_pro usb_device hid-rp logger task timer base_component
    esp_tinyusb tinyusb
    esp_driver_gpio esp_driver_uart esp_timer freertos pthread
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
