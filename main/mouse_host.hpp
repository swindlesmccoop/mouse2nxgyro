#pragma once

// USB mouse input via TinyUSB host on the MAX3421E.
//
// Runs the TinyUSB host stack in its own task and exposes the mouse state
// through lock-free accumulators, so the 15 ms report loop can drain whatever
// motion arrived since its previous tick without blocking on USB.

#include <atomic>
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

/// Start the MAX3421E glue, the TinyUSB host root port, and the host task.
void start();

/// True while a HID mouse interface is mounted and being polled.
bool mounted();

/// Current button mask (Button bits). Level, not edge.
uint8_t buttons();

/// Return and clear the accumulated X / Y motion (mouse counts; HID sign
/// convention: +x right, +y toward the user). Each call hands back everything
/// that arrived since the previous call.
int32_t take_dx();
int32_t take_dy();

/// Return and clear accumulated wheel detents (+ away from the user).
int32_t take_wheel();

/// Number of HID reports received since start (diagnostics: polling-rate check).
uint32_t report_count();

} // namespace m2g::mouse_host
