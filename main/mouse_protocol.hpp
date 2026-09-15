#pragma once

// Line-oriented packets the PC relay writes to the S3 UART (Micro-USB / UART0 RX).
// TX is still ESP_LOG so scripts/relay.py can print firmware logs.
//
//   M <dx> <dy> <buttons>\n     mouse (gyro). buttons unused if a C line is live.
//   C <lx> <ly> <pad_bits>\n    left stick milli-units [-1000,1000] + pad bitmask
//
// 115200 baud is enough for both at 125 Hz.

#include <cstdint>

namespace m2g::protocol {

inline constexpr uint32_t baud = 115200;
inline constexpr char mouse_prefix = 'M';
inline constexpr char pad_prefix = 'C';
inline constexpr char prefix = mouse_prefix; // back-compat alias

inline constexpr uint32_t stale_ms = 250;

inline constexpr uint32_t btn_a = 1u << 0;
inline constexpr uint32_t btn_b = 1u << 1;
inline constexpr uint32_t btn_x = 1u << 2;
inline constexpr uint32_t btn_y = 1u << 3;
inline constexpr uint32_t btn_l = 1u << 4;
inline constexpr uint32_t btn_r = 1u << 5;
inline constexpr uint32_t btn_zl = 1u << 6;
inline constexpr uint32_t btn_zr = 1u << 7;
inline constexpr uint32_t btn_minus = 1u << 8;
inline constexpr uint32_t btn_plus = 1u << 9;
inline constexpr uint32_t btn_l3 = 1u << 10;
inline constexpr uint32_t btn_r3 = 1u << 11;
inline constexpr uint32_t btn_home = 1u << 12;
inline constexpr uint32_t btn_capture = 1u << 13;
inline constexpr uint32_t btn_dpad_up = 1u << 14;
inline constexpr uint32_t btn_dpad_down = 1u << 15;
inline constexpr uint32_t btn_dpad_left = 1u << 16;
inline constexpr uint32_t btn_dpad_right = 1u << 17;

} // namespace m2g::protocol
