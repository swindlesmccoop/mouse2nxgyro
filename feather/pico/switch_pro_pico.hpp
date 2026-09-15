#pragma once

#include "imu_frame.hpp"

#include <cstddef>
#include <cstdint>

struct HidReply {
  uint8_t report_id = 0;
  uint8_t data[63]{};
  bool valid = false;
};

class PicoSwitchPro {
public:
  PicoSwitchPro();

  void on_attach();
  void on_hid_out(uint8_t report_id, const uint8_t *buffer, size_t len);

  bool take_reply(HidReply *out);

  void set_pad(float lx, float ly, uint32_t pad_bits);
  void set_imu(const m2g::ImuFrames &frames);

  void fill_standard_report(uint8_t *out63);
  void increment_counter() { counter_ = static_cast<uint8_t>(counter_ + 1); }

  bool hid_ready() const { return hid_ready_; }
  bool imu_enabled() const { return imu_enabled_; }

private:
  void copy_device_info_81(uint8_t *out63) const;
  void process_command(const uint8_t *data, size_t len);
  void queue_reply(uint8_t report_id, const uint8_t *data63);
  void pack_sticks(uint8_t *analog6, float lx, float ly, float rx, float ry) const;
  void pack_buttons(uint8_t *b2, uint32_t pad) const;
  void apply_housekeeping(uint8_t *out63);
  void write_imu(uint8_t *out63) const;
  uint8_t spi_read_impl(uint8_t bank, uint8_t reg, uint8_t n, uint8_t *dst);

  uint8_t mac_[6];
  uint8_t factory_[256];
  uint8_t user_[128];
  uint8_t pad_body_[12];
  m2g::ImuFrames imu_{};
  uint8_t counter_{0};
  uint8_t vibrator_{0x90};
  uint8_t player_{0};
  bool hid_ready_{false};
  bool imu_enabled_{false};
  bool vibration_enabled_{false};
  uint8_t input_report_mode_{0};

  HidReply pending_{};
};
