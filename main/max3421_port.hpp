#pragma once

// ESP-IDF glue between TinyUSB's MAX3421E host-controller driver (hcd_max3421.c)
// and the SPI bus / interrupt GPIO the breakout is wired to (board_pins.hpp).

namespace m2g::max3421 {

/// TinyUSB root-hub port number the MAX3421E is registered as. The native
/// USB-OTG controller is rhport 0 (device role, the Switch); the external chip
/// is rhport 1. Must match tuh_rhport_init()/tuh_int_handler() calls.
inline constexpr unsigned char rhport = 1;

/// Hardware-reset the chip (if a RESET pin is wired), bring up the SPI bus and
/// device, and arm the INT GPIO interrupt (initially disabled; TinyUSB enables
/// it through tuh_max3421_int_api once hcd_init has configured the chip).
/// Call BEFORE tuh_rhport_init().
void init();

} // namespace m2g::max3421
