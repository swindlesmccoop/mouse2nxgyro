#include "gyro_switch_pro.hpp"

#include <mutex>

namespace m2g {

namespace {

inline void put_i16le(std::vector<uint8_t> &buf, size_t at, int16_t v) {
  const auto u = static_cast<uint16_t>(v);
  buf[at] = static_cast<uint8_t>(u & 0xFF);
  buf[at + 1] = static_cast<uint8_t>(u >> 8);
}

} // namespace

void GyroSwitchPro::set_imu_frames(const ImuFrames &frames) {
  std::lock_guard<std::recursive_mutex> lock(input_report_mutex_);
  frames_ = frames;
}

std::vector<uint8_t> GyroSwitchPro::get_input_report() const {
  // Base class: gamepad bytes, counter, vibrator byte, and (if the IMU is
  // enabled) its placeholder IMU blob. Returns empty until the handshake has
  // reached "USB HID enabled".
  auto report = espp::SwitchPro::get_input_report();
  if (report.empty() || !imu_enabled_.load())
    return report;
  if (report.size() < imu_offset + imu_bytes)
    return report; // cannot happen with the 63-byte hid-rp report; be defensive

  std::lock_guard<std::recursive_mutex> lock(input_report_mutex_);
  size_t at = imu_offset;
  for (const auto &f : frames_) {
    for (int16_t a : f.acc) {
      put_i16le(report, at, a);
      at += 2;
    }
    for (int16_t g : f.gyro) {
      put_i16le(report, at, g);
      at += 2;
    }
  }
  return report;
}

} // namespace m2g
