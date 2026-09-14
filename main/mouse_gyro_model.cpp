#include "mouse_gyro_model.hpp"

#include <algorithm>
#include <cmath>

namespace m2g {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinDt = 0.001f; // guards the first tick / a stalled timer
constexpr float kMaxDt = 0.250f;
} // namespace

void MouseGyroModel::reset() {
  yaw_dps_filt_ = 0.0f;
  pitch_dps_filt_ = 0.0f;
  virtual_pitch_deg_ = 0.0f;
}

int16_t MouseGyroModel::clamp_i16(float v) {
  v = std::clamp(v, -32768.0f, 32767.0f);
  return static_cast<int16_t>(std::lround(v));
}

ImuFrames MouseGyroModel::tick(int32_t dx, int32_t dy, float dt_s) {
  dt_s = std::clamp(dt_s, kMinDt, kMaxDt);

  // Angular velocity that, held constant for dt, rotates by exactly
  // counts * deg_per_count. This is the improvement over the prior art (NXIC,
  // SwitchProconGyroMouse) which scale each raw mouse packet by a constant and
  // therefore depend on the mouse's poll rate.
  const float yaw_dps = static_cast<float>(dx) * cfg_.deg_per_count / dt_s;
  const float pitch_dps = static_cast<float>(dy) * cfg_.deg_per_count / dt_s;
  return frames_for_rates(yaw_dps, pitch_dps, dt_s);
}

ImuFrames MouseGyroModel::frames_for_rates(float yaw_dps, float pitch_dps, float dt_s) {
  dt_s = std::clamp(dt_s, kMinDt, kMaxDt);

  if (cfg_.smoothing > 0.0f) {
    const float a = std::clamp(cfg_.smoothing, 0.0f, 0.99f);
    yaw_dps_filt_ = a * yaw_dps_filt_ + (1.0f - a) * yaw_dps;
    pitch_dps_filt_ = a * pitch_dps_filt_ + (1.0f - a) * pitch_dps;
    yaw_dps = yaw_dps_filt_;
    pitch_dps = pitch_dps_filt_;
  }

  yaw_dps = std::clamp(yaw_dps, -cfg_.max_dps, cfg_.max_dps);
  pitch_dps = std::clamp(pitch_dps, -cfg_.max_dps, cfg_.max_dps);

  // Track the pitch we have told the Switch about (used by AccelMode::TrackedPitch).
  virtual_pitch_deg_ =
      std::clamp(virtual_pitch_deg_ + cfg_.pitch_sign * pitch_dps * dt_s,
                 -cfg_.max_virtual_pitch_deg, cfg_.max_virtual_pitch_deg);

  ImuFrame f{};
  // Gyro at calibrated zero on every axis, then the two we drive.
  for (size_t i = 0; i < 3; ++i)
    f.gyro[i] = cfg_.cal.gyro_offset[i];
  f.gyro[cfg_.yaw_axis] =
      clamp_i16(cfg_.cal.gyro_raw_from_dps(cfg_.yaw_axis, cfg_.yaw_sign * yaw_dps));
  f.gyro[cfg_.pitch_axis] =
      clamp_i16(cfg_.cal.gyro_raw_from_dps(cfg_.pitch_axis, cfg_.pitch_sign * pitch_dps));
  fill_accel(f);

  // The report's three frames are nominally 5 ms apart; a constant rate across
  // the window is exactly what we are modelling, so all three are identical.
  return ImuFrames{f, f, f};
}

void MouseGyroModel::fill_accel(ImuFrame &f) const {
  float gx = 0.0f, gy = 0.0f, gz = 1.0f; // lying flat: +1 G "up" on Z
  if (cfg_.accel_mode == AccelMode::TrackedPitch) {
    // Nose-down by p (positive gyro Y integrated): the world "up" vector gains a
    // component along the body's forward (+X) axis. Sign to be confirmed on a
    // real game; flip gx if the fused horizon moves the wrong way.
    const float p = virtual_pitch_deg_ * kPi / 180.0f;
    gx = std::sin(p);
    gz = std::cos(p);
  }
  f.acc[Axis::X] = clamp_i16(cfg_.cal.acc_raw_from_g(Axis::X, gx));
  f.acc[Axis::Y] = clamp_i16(cfg_.cal.acc_raw_from_g(Axis::Y, gy));
  f.acc[Axis::Z] = clamp_i16(cfg_.cal.acc_raw_from_g(Axis::Z, gz));
}

} // namespace m2g

// Host-side sanity check (no ESP-IDF needed), from the repo root:
//
//   g++ -std=c++20 -Imain -x c++ - <<'EOF'
//   #include "mouse_gyro_model.hpp"
//   #include "detail/switch_pro_spi_rom_data.hpp"   // add -Iexternal/espp/components/switch_pro/include
//   #include <cassert>
//   #include <cstdio>
//   int main() {
//     using namespace m2g;
//     MouseGyroModel::Config c;
//     c.cal = parse_imu_calibration(sp::spi_rom_data_80, 0x28);
//     MouseGyroModel m(c);
//     auto idle = m.tick(0, 0, 0.015f);
//     assert(idle[0].gyro[2] == c.cal.gyro_offset[2]);          // idle == calibrated zero
//     auto turn = m.tick(15, 0, 0.015f);                         // 1.5 deg in 15 ms = 100 dps
//     float dps = (turn[0].gyro[2] - c.cal.gyro_offset[2]) * 936.0f / (c.cal.gyro_sens[2] - c.cal.gyro_offset[2]);
//     std::printf("yaw raw=%d -> %.1f dps (expect -100)\n", turn[0].gyro[2], dps);
//     assert(std::abs(dps + 100.0f) < 0.5f);
//     return 0;
//   }
//   EOF
