#pragma once

// ESP32-S3-USB-OTG board bring-up: USB routing, 5 V distribution, LEDs, buttons.
// Pin numbers live in board_pins.hpp.

namespace board {

/// Configure the board for "Pro Controller on USB_DEV, MAX3421E powered from the
/// USB_HOST receptacle" operation:
///  - USB_SEL low     : native USB PHY on the USB_DEV plug (device role)
///  - BOOST_EN low    : never boost from a battery
///  - DEV_VBUS_EN high: pass the dock's 5 V onward
///  - LIMIT_EN high   : enable the 500 mA-limited VBUS on the USB_HOST receptacle
///  - LEDs as outputs (off), buttons as inputs (board has external pull-ups).
void init();

void set_led_green(bool on);
void set_led_yellow(bool on);

/// Buttons are active-low; these return true while pressed.
bool button_ok();
bool button_up();
bool button_down();
bool button_menu();

/// True while the USB_HOST VBUS current limiter reports an over-current condition.
bool over_current();

} // namespace board
