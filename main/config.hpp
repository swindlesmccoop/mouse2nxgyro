#pragma once

// User-facing tunables. Everything you are likely to want to change lives here.

#include <cstdint>

#include "mouse_gyro_model.hpp"
#include "mouse_host.hpp"

namespace m2g::config {

// --- Report cadence -----------------------------------------------------------------
/// A real Pro Controller streams a 0x30 report every 15 ms over USB
/// (three 5 ms IMU frames per report). Keep it.
inline constexpr int report_period_ms = 15;

// --- Mouse -> gyro ---------------------------------------------------------------------
/// Degrees of virtual controller rotation per mouse count. THE sensitivity knob.
///
///   counts per 360 deg = 360 / deg_per_count
///   cm per 360 deg     = counts per 360 deg / (mouse DPI / 2.54)
///
/// e.g. 0.1 deg/count @ 400 DPI  -> 3600 counts -> 22.9 cm per full turn;
///      0.1 deg/count @ 1600 DPI ->              -> 5.7 cm per full turn.
/// The game's own gyro sensitivity setting multiplies on top of this. Start
/// here, then tune with the game's slider before touching this number.
inline constexpr float deg_per_count = 0.1f;

/// Flip mouse Y (pull toward you = look up instead of look down).
inline constexpr bool invert_y = false;

/// Exponential smoothing of angular velocity, 0 = off. Try 0.3-0.5 on a 125 Hz
/// mouse if aiming feels steppy; leave 0 for a 500/1000 Hz mouse.
inline constexpr float smoothing = 0.0f;

/// Accelerometer strategy. ConstantGravity is the safe default; TrackedPitch is
/// the physically-consistent experiment (see MouseGyroModel::AccelMode).
inline constexpr MouseGyroModel::AccelMode accel_mode = MouseGyroModel::AccelMode::ConstantGravity;

// --- Synthetic IMU self-test (board buttons UP / DW) -------------------------------------
/// While enabled, holding UP on the board injects a steady yaw at
/// synthetic_dps (as if the mouse moved right), holding DW injects a steady
/// pitch (as if the mouse moved down / toward you). Open a gyro-aim game, hold
/// a button, and check the camera turns the expected way at a plausible speed.
/// Set to false once the axis/sign/scale have been confirmed.
inline constexpr bool synthetic_sweep_enabled = true;
inline constexpr float synthetic_dps = 90.0f;

// --- Mouse / keyboard binds ----------------------------------------------------------
/// Keyboard + mouse-button -> Pro Controller mapping lives in
/// `config/binds.json` (loaded by scripts/relay.py). Restart the relay after
/// edits, or wait ~2 s for a reload. Fallback below is only used if no C-line
/// has arrived (old relay).
struct MouseButtonMap {
  uint8_t l = mouse_host::Left;
  uint8_t r = mouse_host::Right;
  uint8_t zl = mouse_host::Forward;
  uint8_t zr = mouse_host::Back;
  uint8_t thumb_r = mouse_host::Middle;
};
inline constexpr MouseButtonMap mouse_buttons{};

// --- Board buttons -------------------------------------------------------------------
// OK   = A               (confirm; gets you through the "controller connected" screens)
// MENU = HOME
// UP   = synthetic yaw   (while synthetic_sweep_enabled)
// DW   = synthetic pitch (while synthetic_sweep_enabled)
// UP+DW together = L + R (the "Change Grip/Order" pairing prompt)

} // namespace m2g::config
