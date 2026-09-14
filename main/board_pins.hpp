#pragma once

// Pin map for the Espressif ESP32-S3-USB-OTG development board.
//
// Source: ESP32-S3-USB-OTG user guide, "Pin Layout" (function pins + extended
// pins) and the "USB Interface Switch Circuit" / "USB HOST Interface Power
// Options" sections:
// https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-usb-otg/user_guide.html
//
// Only the pins this firmware touches are listed. The LCD (GPIO4-9) and SD card
// (GPIO33-38) are left alone.

#include "driver/gpio.h"
#include "driver/spi_common.h" // spi_host_device_t

namespace board::pins {

// --- USB routing and power -----------------------------------------------------
//
// The board has ONE USB-OTG PHY (GPIO19 D-, GPIO20 D+) behind a TS3USB30E
// analog switch. USB_SEL picks which connector it reaches:
//   low  (default) -> USB_DEV  (Type-A MALE plug)   : ESP32-S3 is a USB DEVICE
//   high           -> USB_HOST (Type-A FEMALE port) : ESP32-S3 is a USB HOST
// We are the Pro Controller, so USB_SEL stays LOW and the board plugs into the
// Switch dock with the USB_DEV plug. The mouse is NOT on this PHY at all; it is
// on the external MAX3421E (below).
inline constexpr gpio_num_t usb_sel = GPIO_NUM_18;

// 5 V distribution. With DEV_VBUS_EN high (and BOOST_EN low) the 5 V arriving on
// the USB_DEV plug (from the dock) is passed through the MIC2005A current limiter
// (enabled by LIMIT_EN) to the USB_HOST receptacle's VBUS pin. That receptacle is
// otherwise unused by this firmware, so its current-limited 5 V is a convenient,
// protected supply for the MAX3421E breakout's downstream USB port (the mouse).
inline constexpr gpio_num_t dev_vbus_en = GPIO_NUM_12; // high: route USB_DEV 5 V onward
inline constexpr gpio_num_t boost_en = GPIO_NUM_13;    // high: battery boost (must be LOW here)
inline constexpr gpio_num_t limit_en = GPIO_NUM_17;    // high: enable 500 mA limiter output
inline constexpr gpio_num_t over_current_flag = GPIO_NUM_21; // input, high = limiter tripped

// --- LEDs (active high) -----------------------------------------------------------
inline constexpr gpio_num_t led_green = GPIO_NUM_15;  // lit: Switch has enabled input reports
inline constexpr gpio_num_t led_yellow = GPIO_NUM_16; // lit: a USB mouse is mounted

// --- Buttons (active low, external pull-ups on the board) ---------------------------
inline constexpr gpio_num_t btn_ok = GPIO_NUM_0;    // also the BOOT strapping pin
inline constexpr gpio_num_t btn_dw = GPIO_NUM_11;
inline constexpr gpio_num_t btn_up = GPIO_NUM_10;
inline constexpr gpio_num_t btn_menu = GPIO_NUM_14;

// --- MAX3421E USB host controller (external breakout, wired to the free pins) ------
//
// The user guide lists six idle "extended" pins: GPIO45, 46, 48, 26, 47, 3.
// Assignment rule: signals the ESP32 DRIVES (MAX3421E inputs) go on the strapping
// pins GPIO45 / GPIO46 / GPIO3, so nothing external can pull a strapping pin the
// wrong way while the chip is in reset (GPIO45 selects the VDD_SPI voltage; a
// high level there at reset would misconfigure flash power). Signals the
// MAX3421E DRIVES (MISO, INT) go on plain GPIOs.
//
// Adafruit "USB Host FeatherWing" pin names in brackets.
inline constexpr gpio_num_t max_sck = GPIO_NUM_45;  // [SCK]  ESP -> MAX
inline constexpr gpio_num_t max_mosi = GPIO_NUM_46; // [MO]   ESP -> MAX
inline constexpr gpio_num_t max_cs = GPIO_NUM_48;   // [CS]   ESP -> MAX, active low
inline constexpr gpio_num_t max_miso = GPIO_NUM_47; // [MI]   MAX -> ESP
inline constexpr gpio_num_t max_int = GPIO_NUM_26;  // [IRQ]  MAX -> ESP, active low
// [RST] optional: drive low then high at boot. Set to GPIO_NUM_NC if the
// breakout's RST is tied high / has its own pull-up and you leave it unwired.
inline constexpr gpio_num_t max_reset = GPIO_NUM_3;

// SPI peripheral for the MAX3421E. SPI2 is wired to the LCD on this board;
// use SPI3 so the display could be added later without a bus conflict.
inline constexpr spi_host_device_t max_spi_host = SPI3_HOST;

} // namespace board::pins
