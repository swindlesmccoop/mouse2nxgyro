#pragma once

// Line-oriented mouse packets the PC relay writes to the S3 UART (Micro-USB /
// UART0 RX). TX is still ESP_LOG so scripts/relay.py can print firmware logs
// while stuffing these lines the other way.
//
// One line per 8 ms (see feather/mouse_relay):
//   M <dx> <dy> <buttons>\n
//
// dx, dy: signed HID counts accumulated since the previous line (+x right, +y
// toward the user). buttons: HID boot-protocol bit mask (same as mouse_host::Button).
//
// 115200 baud is enough: ~20 bytes * 125 Hz ≈ 2.5 kB/s.

#include <cstdint>

namespace m2g::protocol {

inline constexpr uint32_t baud = 115200;
inline constexpr char prefix = 'M';

/// If no valid line arrives for this long, treat the mouse as unplugged.
inline constexpr uint32_t stale_ms = 250;

} // namespace m2g::protocol
