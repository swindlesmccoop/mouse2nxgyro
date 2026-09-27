// Switch Pro Controller protocol engine for the RP2040 (no FreeRTOS / esp-idf).
//
// Ported from esp-cpp/espp components/switch_pro (src/switch_pro.cpp and
// src/protocol.cpp: handshake, subcommand replies, SPI-flash reads), espp commit
// 038eea4a63d3bc4c287b6c1420924a646abb5ee2, MIT License, Copyright (c) 2022
// esp-cpp. espp in turn follows Brikwerk/nxbt nxbt/controller/protocol.py (MIT).
// Protocol constants and SPI ROM data are included from espp unmodified.
// Changes: plain C++ without espp's task/timer/logger, a single pending reply
// instead of a queue, deterministic serial and MAC, IMU frames from the mouse
// model, and user stick calibration cleared (see the constructor).
// See CREDITS.md.

#include "switch_pro_pico.hpp"

#include "detail/switch_controller_protocol.hpp"
#include "detail/switch_pro_spi_rom_data.hpp"
#include "mouse_protocol.hpp"

#include <algorithm>
#include <cstring>

namespace {

void pack12(uint8_t *d, uint16_t x, uint16_t y) {
  d[0] = static_cast<uint8_t>(x & 0xFF);
  d[1] = static_cast<uint8_t>(((x >> 8) & 0x0F) | ((y & 0x0F) << 4));
  d[2] = static_cast<uint8_t>((y >> 4) & 0xFF);
}

uint16_t stick_from_float(float v) {
  v = std::clamp(v, -1.0f, 1.0f);
  const float raw = v * 2048.0f + 2048.0f;
  int n = static_cast<int>(raw);
  if (n < 0)
    n = 0;
  // 12-bit field: 4096 would wrap to 0 in pack12() and read as full negative.
  if (n > 4095)
    n = 4095;
  return static_cast<uint16_t>(n);
}

void put_i16le(uint8_t *p, int16_t v) {
  const uint16_t u = static_cast<uint16_t>(v);
  p[0] = static_cast<uint8_t>(u & 0xFF);
  p[1] = static_cast<uint8_t>(u >> 8);
}

} // namespace

PicoSwitchPro::PicoSwitchPro() {
  std::memcpy(factory_, sp::spi_rom_data_60, sizeof(sp::spi_rom_data_60));
  if (sizeof(sp::spi_rom_data_60) < sizeof(factory_))
    std::memset(factory_ + sizeof(sp::spi_rom_data_60), 0xFF,
                sizeof(factory_) - sizeof(sp::spi_rom_data_60));
  std::memcpy(user_, sp::spi_rom_data_80, sizeof(sp::spi_rom_data_80));
  if (sizeof(sp::spi_rom_data_80) < sizeof(user_))
    std::memset(user_ + sizeof(sp::spi_rom_data_80), 0xFF, sizeof(user_) - sizeof(sp::spi_rom_data_80));

  // The upstream user stick calibration has a zero range below center, and the
  // Switch prefers user cal over factory when its magic is present, so every
  // deflection lands on the negative side. Clear both stick magics so the
  // factory calibration (center 2048, +2047/-2048) is used instead.
  std::memset(user_ + sp::REG_USER_ANALOG_START, 0xFF, 2);
  std::memset(user_ + sp::REG_USER_ANALOG_START + 11, 0xFF, 2);

  static constexpr char kSerial[] = "00000000001";
  std::memcpy(factory_, kSerial, 11);
  std::memset(factory_ + 11, 0, 5);

  mac_[0] = 0x98;
  mac_[1] = 0xB6;
  mac_[2] = 0xE9;
  mac_[3] = 0xEC;
  mac_[4] = 0x5E;
  mac_[5] = 0xFD;

  std::memset(pad_body_, 0, sizeof(pad_body_));
  apply_housekeeping(pad_body_);
  pack_sticks(pad_body_ + 5, 0, 0, 0, 0);
  pad_body_[11] = vibrator_;
}

void PicoSwitchPro::on_attach() {
  hid_ready_ = false;
  imu_enabled_ = false;
  vibrator_ = 0x90;
  input_report_mode_ = 0;
  vibration_enabled_ = false;
  player_ = 0;
  pending_.valid = false;

  uint8_t resp[63]{};
  copy_device_info_81(resp);
  queue_reply(sp::DEVICE_INIT_REPORT, resp);
}

void PicoSwitchPro::copy_device_info_81(uint8_t *out63) const {
  std::memcpy(out63, sp::device_init_report_data, sizeof(sp::device_init_report_data));
  out63[sp::device_init_report_data_mac_addr_offset + 0] = mac_[5];
  out63[sp::device_init_report_data_mac_addr_offset + 1] = mac_[4];
  out63[sp::device_init_report_data_mac_addr_offset + 2] = mac_[3];
  out63[sp::device_init_report_data_mac_addr_offset + 3] = mac_[2];
  out63[sp::device_init_report_data_mac_addr_offset + 4] = mac_[1];
  out63[sp::device_init_report_data_mac_addr_offset + 5] = mac_[0];
}

void PicoSwitchPro::queue_reply(uint8_t report_id, const uint8_t *data63) {
  pending_.report_id = report_id;
  std::memcpy(pending_.data, data63, 63);
  pending_.valid = true;
}

bool PicoSwitchPro::take_reply(HidReply *out) {
  if (!pending_.valid)
    return false;
  *out = pending_;
  pending_.valid = false;
  return true;
}

void PicoSwitchPro::on_hid_out(uint8_t report_id, const uint8_t *buffer, size_t len) {
  uint8_t tmp[64];
  const uint8_t *data = buffer;
  size_t n = len;
  if (report_id != 0 && (len == 0 || buffer[0] != report_id)) {
    if (len + 1 > sizeof(tmp))
      return;
    tmp[0] = report_id;
    if (len)
      std::memcpy(tmp + 1, buffer, len);
    data = tmp;
    n = len + 1;
  }
  if (data == nullptr || n == 0)
    return;

  using namespace sp;
  switch (data[0]) {
  case HOST_INIT_REPORT: {
    if (n < 2)
      return;
    uint8_t cmd = data[1];
    uint8_t resp[63]{};
    resp[0] = cmd;
    switch (cmd) {
    case INIT_COMMAND_DEVICE_INFO:
      copy_device_info_81(resp);
      break;
    case INIT_COMMAND_HANDSHAKE: {
      const size_t copy_n = std::min(n - 1, sizeof(resp));
      std::memcpy(resp, data + 1, copy_n);
      break;
    }
    case INIT_COMMAND_SET_BAUD_RATE:
      break;
    case INIT_COMMAND_ENABLE_USB_HID:
      hid_ready_ = true;
      break;
    case INIT_COMMAND_ENABLE_BT_HID:
      break;
    default:
      break;
    }
    queue_reply(DEVICE_INIT_REPORT, resp);
    return;
  }
  case HOST_OUTPUT_REPORT:
    process_command(data, n);
    return;
  case HOST_RUMBLE_REPORT:
    return;
  default:
    return;
  }
}

void PicoSwitchPro::process_command(const uint8_t *data, size_t len) {
  sp::Message message(data, len);
  uint8_t report[63]{};
  fill_standard_report(report);
  report[12] = 0x80;
  report[13] = message.subcommand_id;
  report[14] = 0;

  switch (message.response) {
  case sp::Response::ONLY_CONTROLLER_STATE:
    report[12] = 0x80;
    report[13] = 0x00;
    break;
  case sp::Response::BT_MANUAL_PAIRING:
    report[12] = 0x81;
    report[13] = 0x01;
    break;
  case sp::Response::REQUEST_DEVICE_INFO:
    hid_ready_ = true;
    report[12] = 0x82;
    report[13] = 0x02;
    std::memcpy(report + 14, sp::device_info, sizeof(sp::device_info));
    std::memcpy(report + 18, mac_, 6);
    break;
  case sp::Response::SET_SHIPMENT:
    report[12] = 0x80;
    report[13] = 0x08;
    break;
  case sp::Response::SPI_READ: {
    if (message.subcommand_len < 6) {
      report[12] = 0x83;
      report[13] = 0x00;
      break;
    }
    uint8_t addr_bottom = message.subcommand[1];
    uint8_t addr_top = message.subcommand[2];
    uint8_t read_length = message.subcommand[5];
    if (read_length > 63 - 19)
      read_length = static_cast<uint8_t>(63 - 19);
    const uint8_t got = spi_read_impl(addr_top, addr_bottom, read_length, report + 19);
    if (got > 0) {
      report[12] = 0x90;
      report[13] = 0x10;
      report[14] = addr_bottom;
      report[15] = addr_top;
      report[16] = 0;
      report[17] = 0;
      report[18] = got;
    } else {
      report[12] = 0x83;
      report[13] = 0x00;
    }
    break;
  }
  case sp::Response::SET_MODE:
    report[12] = 0x80;
    report[13] = 0x03;
    if (message.subcommand_len > 1)
      input_report_mode_ = message.subcommand[1];
    break;
  case sp::Response::TRIGGER_BUTTONS_ELAPSED:
    report[12] = 0x83;
    report[13] = 0x04;
    std::memset(report + 14, 0, 14);
    break;
  case sp::Response::TOGGLE_IMU:
    imu_enabled_ = (message.subcommand_len > 1 && message.subcommand[1] == 0x01);
    report[12] = 0x80;
    report[13] = 0x40;
    break;
  case sp::Response::ENABLE_VIBRATION:
    report[12] = 0x82;
    report[13] = 0x48;
    vibration_enabled_ = true;
    break;
  case sp::Response::SET_PLAYER: {
    report[12] = 0x80;
    report[13] = 0x30;
    const uint8_t bitfield = (message.subcommand_len > 1) ? message.subcommand[1] : 0;
    if (bitfield == 0x01 || bitfield == 0x10)
      player_ = 1;
    else if (bitfield == 0x03 || bitfield == 0x30)
      player_ = 2;
    else if (bitfield == 0x07 || bitfield == 0x70)
      player_ = 3;
    else if (bitfield == 0x0F || bitfield == 0xF0)
      player_ = 4;
    break;
  }
  case sp::Response::SET_NFC_IR_STATE:
    report[12] = 0x80;
    report[13] = 0x22;
    break;
  case sp::Response::SET_NFC_IR_CONFIG: {
    report[12] = 0xA0;
    report[13] = 0x21;
    static constexpr uint8_t params[] = {0x01, 0x00, 0xFF, 0x00, 0x08, 0x00, 0x1B, 0x01};
    std::memcpy(report + 14, params, sizeof(params));
    report[47] = 0xC8;
    break;
  }
  default:
    report[12] = 0x80;
    report[13] = message.subcommand_id;
    report[14] = 0x03;
    break;
  }

  static constexpr uint8_t vib[] = {0xA0, 0xB0, 0xC0, 0x90};
  vibrator_ = vib[counter_ & 3];
  report[0] = counter_;
  report[11] = vibrator_;
  queue_reply(sp::DEVICE_RESPONSE_REPORT, report);
}

uint8_t PicoSwitchPro::spi_read_impl(uint8_t bank, uint8_t reg, uint8_t n, uint8_t *dst) {
  using namespace sp;
  auto from = [&](const uint8_t *src, size_t src_size) -> uint8_t {
    if (reg >= src_size)
      return 0;
    const size_t avail = src_size - reg;
    const uint8_t got = static_cast<uint8_t>(std::min<size_t>(n, avail));
    std::memcpy(dst, src + reg, got);
    return got;
  };
  if (bank == REG_BANK_SHIPMENT) {
    std::memset(dst, 0, n);
    return n;
  }
  if (bank == REG_BANK_FACTORY_CONFIG)
    return from(factory_, sizeof(factory_));
  if (bank == REG_BANK_USER_CAL)
    return from(user_, sizeof(user_));
  return 0;
}

void PicoSwitchPro::apply_housekeeping(uint8_t *out63) {
  // hid-rp: connection_info:4 | battery_charging:1 | battery_level:3
  out63[1] = static_cast<uint8_t>(0x01 | (1u << 4) | (4u << 5));
}

void PicoSwitchPro::pack_sticks(uint8_t *analog6, float lx, float ly, float rx, float ry) const {
  pack12(analog6, stick_from_float(lx), stick_from_float(ly));
  pack12(analog6 + 3, stick_from_float(rx), stick_from_float(ry));
}

void PicoSwitchPro::pack_buttons(uint8_t *b2, uint32_t pad) const {
  using namespace m2g::protocol;
  uint8_t b0 = 0, b1 = 0, b2v = 0;
  if (pad & btn_y)
    b0 |= 1u << 0;
  if (pad & btn_x)
    b0 |= 1u << 1;
  if (pad & btn_b)
    b0 |= 1u << 2;
  if (pad & btn_a)
    b0 |= 1u << 3;
  if (pad & btn_r)
    b0 |= 1u << 6;
  if (pad & btn_zr)
    b0 |= 1u << 7;
  if (pad & btn_minus)
    b1 |= 1u << 0;
  if (pad & btn_plus)
    b1 |= 1u << 1;
  if (pad & btn_r3)
    b1 |= 1u << 2;
  if (pad & btn_l3)
    b1 |= 1u << 3;
  if (pad & btn_home)
    b1 |= 1u << 4;
  if (pad & btn_capture)
    b1 |= 1u << 5;
  if (pad & btn_dpad_down)
    b2v |= 1u << 0;
  if (pad & btn_dpad_up)
    b2v |= 1u << 1;
  if (pad & btn_dpad_right)
    b2v |= 1u << 2;
  if (pad & btn_dpad_left)
    b2v |= 1u << 3;
  if (pad & btn_l)
    b2v |= 1u << 6;
  if (pad & btn_zl)
    b2v |= 1u << 7;
  b2[0] = b0;
  b2[1] = b1;
  b2[2] = b2v;
}

void PicoSwitchPro::set_pad(float lx, float ly, uint32_t pad_bits) {
  apply_housekeeping(pad_body_);
  pack_buttons(pad_body_ + 2, pad_bits);
  pack_sticks(pad_body_ + 5, lx, ly, 0.0f, 0.0f);
  pad_body_[11] = vibrator_;
}

void PicoSwitchPro::set_imu(const m2g::ImuFrames &frames) { imu_ = frames; }

void PicoSwitchPro::write_imu(uint8_t *out63) const {
  if (!imu_enabled_)
    return;
  uint8_t *p = out63 + 12;
  for (const auto &f : imu_) {
    for (int16_t a : f.acc) {
      put_i16le(p, a);
      p += 2;
    }
    for (int16_t g : f.gyro) {
      put_i16le(p, g);
      p += 2;
    }
  }
}

void PicoSwitchPro::fill_standard_report(uint8_t *out63) {
  std::memset(out63, 0, 63);
  std::memcpy(out63, pad_body_, 12);
  out63[0] = counter_;
  apply_housekeeping(out63);
  out63[11] = vibrator_;
  write_imu(out63);
}
