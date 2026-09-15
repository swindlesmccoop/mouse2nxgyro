#include "board.hpp"

#include "board_pins.hpp"

#include "driver/gpio.h"

namespace board {

namespace {

void configure_output(gpio_num_t pin, bool level) {
  gpio_reset_pin(pin);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_set_level(pin, level ? 1 : 0);
}

void configure_input(gpio_num_t pin, bool pull_up) {
  gpio_reset_pin(pin);
  gpio_set_direction(pin, GPIO_MODE_INPUT);
  gpio_set_pull_mode(pin, pull_up ? GPIO_PULLUP_ONLY : GPIO_FLOATING);
}

} // namespace

void init() {
  using namespace pins;

  // USB routing: keep the single PHY on the USB_DEV plug (device role). This is
  // the power-on default (USB_SEL is pulled low on the board) but drive it
  // explicitly so nothing can flip it.
  configure_output(usb_sel, false);

  // Relay branch: mouse is on the Feather, not USB_HOST. Do not route dock 5 V
  // into the unused host jack (that was for a MAX3421E mouse).
  configure_output(boost_en, false);
  configure_output(dev_vbus_en, false);
  configure_output(limit_en, false);
  // MIC2005 FLAG is open-drain, active low. Board has a pull-up; idle = high.
  configure_input(over_current_flag, true);

  // LEDs off until the respective link is up.
  configure_output(led_green, false);
  configure_output(led_yellow, false);

  // Buttons: the board has external pull-ups; add the internal ones too so a
  // floating read is impossible.
  configure_input(btn_ok, true);
  configure_input(btn_up, true);
  configure_input(btn_dw, true);
  configure_input(btn_menu, true);
}

void set_led_green(bool on) { gpio_set_level(pins::led_green, on ? 1 : 0); }
void set_led_yellow(bool on) { gpio_set_level(pins::led_yellow, on ? 1 : 0); }

bool button_ok() { return gpio_get_level(pins::btn_ok) == 0; }
bool button_up() { return gpio_get_level(pins::btn_up) == 0; }
bool button_down() { return gpio_get_level(pins::btn_dw) == 0; }
bool button_menu() { return gpio_get_level(pins::btn_menu) == 0; }

bool over_current() { return gpio_get_level(pins::over_current_flag) == 0; }

} // namespace board
