#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "switch_pro.hpp"

#include "imu_frame.hpp"

namespace m2g {

/// espp::SwitchPro with a real IMU data path.
///
/// Why this exists (espp @ 038eea4): `SwitchPro::set_imu_data()` writes a fixed
/// 36-byte "controller at rest" blob into bytes 12..47 of every streamed 0x30
/// report whenever the Switch has enabled the IMU (subcommand 0x40), and it runs
/// AFTER `input_report_.get_report()` inside `get_input_report()`, so anything an
/// application put in the report's IMU fields would be overwritten. On top of
/// that, hid-rp's `SwitchProGamepadInputReport` keeps its IMU fields `protected`
/// with no setter. There is therefore no supported way to emit live IMU data.
///
/// This subclass keeps the whole proven handshake/subcommand engine untouched and
/// only replaces the IMU bytes of the streamed report with the frames supplied by
/// `set_imu_frames()`. It can go away once espp grows a `set_imu_frames()`-style
/// API (worth upstreaming).
///
/// Thread-safety: `set_imu_frames()` is called from the sender task;
/// `get_input_report()` from the same task. Both take the base class's
/// `input_report_mutex_` anyway so a future split across tasks stays safe.
class GyroSwitchPro : public espp::SwitchPro {
public:
  using espp::SwitchPro::SwitchPro;

  /// Byte offset of the first IMU frame inside the 63-byte 0x30 report payload
  /// (report id excluded). dekuNukem counts from the report id at byte 0 and
  /// puts accel_x at 13-14; without the id that is 12.
  static constexpr size_t imu_offset = 12;
  static constexpr size_t imu_bytes = 3 * 12; // 3 frames x (3 acc + 3 gyro) x int16

  /// Store the three IMU frames to be sent with the next 0x30 report.
  void set_imu_frames(const ImuFrames &frames);

  /// Whether the Switch has enabled the 6-axis sensor (subcommand 0x40 0x01).
  /// Until it has, a real controller sends zeros in the IMU area, and so do we.
  bool is_imu_enabled() const { return imu_enabled_.load(); }

  /// The 0x30 report payload with OUR IMU frames instead of the base class's
  /// placeholder. Shadows (does not override; the base method is non-virtual)
  /// `espp::SwitchPro::get_input_report()`; always call it through this type.
  std::vector<uint8_t> get_input_report() const;

private:
  ImuFrames frames_{}; // guarded by input_report_mutex_ (base class)
};

} // namespace m2g
