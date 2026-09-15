#pragma once

// Mouse input as seen by the 15 ms gyro loop.
//
// On the `relay` branch this is filled from UART0 (the board's Micro-USB
// CP2102N): a PC process copies packets from the Feather's USB-C serial onto
// this UART. Same lock-free accumulators as the MAX3421E host path on main.

#include <cstdint>

namespace m2g::mouse_host {

/// HID boot-protocol button bits (byte 0 of the mouse report).
enum Button : uint8_t {
  Left = 1 << 0,
  Right = 1 << 1,
  Middle = 1 << 2,
  Back = 1 << 3,
  Forward = 1 << 4,
};

void start();

/// True while the relay is sending packets (Feather has a mouse and the PC
/// script is forwarding). Goes false ~250 ms after the last valid line.
bool mounted();

uint8_t buttons();
int32_t take_dx();
int32_t take_dy();
int32_t take_wheel();
uint32_t report_count();

/// True while a C-line (keyboard/mouse-mapped pad) arrived within stale_ms.
bool pad_live();
float stick_lx();
float stick_ly();
uint32_t pad_buttons();

} // namespace m2g::mouse_host
