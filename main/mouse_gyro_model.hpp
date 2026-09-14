#pragma once

// Mouse deltas -> Switch IMU frames. Pure C++, no ESP-IDF dependencies, so it can
// be unit-tested on a PC (see the comment block at the bottom of the .cpp).

#include <cstdint>

#include "imu_calibration.hpp"
#include "imu_frame.hpp"

namespace m2g {

class MouseGyroModel {
public:
  enum class AccelMode {
    /// Accelerometer always reports the controller lying flat: (0, 0, +1 G).
    /// Simple, drift-free, and what NXIC-style tools effectively do. Games that
    /// fuse accel with gyro for pitch may fight it slightly.
    ConstantGravity,
    /// Integrate the virtual pitch we have been reporting and tilt the gravity
    /// vector to match, so gyro and accel describe the same physical motion.
    /// Experimental; test on a real game before defaulting to it.
    TrackedPitch,
  };

  struct Config {
    ImuCalibration cal{};

    /// Degrees of controller rotation per mouse count. Sensitivity knob.
    /// With a 400 DPI mouse, 0.1 deg/count is 3600 counts per 360 deg, i.e.
    /// 9 in (22.9 cm) of mouse travel for a full turn -- before the game's own
    /// gyro sensitivity setting, which multiplies this.
    float deg_per_count = 0.1f;

    /// Axis/sign convention (see imu_frame.hpp). Defaults follow SDL's remap and
    /// NXIC: mouse right = yaw right = NEGATIVE gyro Z; mouse toward the player
    /// (+dy, "down") = pitch nose-down = POSITIVE gyro Y. Flip a sign if a game
    /// turns the wrong way.
    Axis yaw_axis = Axis::Z;
    float yaw_sign = -1.0f;
    Axis pitch_axis = Axis::Y;
    float pitch_sign = +1.0f;

    /// Maximum angular velocity reported (deg/s). The Switch's gyro range is
    /// +/-2000 dps; clamp below the int16 limit so a violent flick saturates
    /// cleanly instead of wrapping.
    float max_dps = 1800.0f;

    /// Exponential smoothing of the angular velocity, 0 = off (exact
    /// integration), 1 = frozen. Trades a little latency for less jitter on
    /// low-poll-rate mice. Off by default: it makes integrated rotation lag the
    /// mouse and undermines the "counts * deg_per_count exactly" guarantee.
    float smoothing = 0.0f;

    AccelMode accel_mode = AccelMode::ConstantGravity;
    /// TrackedPitch only: keep the virtual pitch within +/- this many degrees so
    /// the gravity vector never flips over.
    float max_virtual_pitch_deg = 85.0f;
  };

  explicit MouseGyroModel(const Config &cfg) : cfg_(cfg) { reset(); }

  const Config &config() const { return cfg_; }
  void set_config(const Config &cfg) { cfg_ = cfg; }

  /// Forget smoothing/pitch state (e.g. when the mouse is unplugged).
  void reset();

  /// Produce the three IMU frames for one 0x30 report.
  ///
  /// \param dx, dy  mouse counts accumulated since the previous tick (HID
  ///                convention: +x right, +y toward the user)
  /// \param dt_s    measured seconds since the previous tick
  ///
  /// The whole window is reported as ONE constant angular velocity
  /// (counts * deg_per_count / dt) in all three frames. Integrating that over dt
  /// gives exactly counts * deg_per_count regardless of how many USB polls the
  /// mouse delivered in the window, so rotation is a pure function of mouse
  /// travel. With dx == dy == 0 the gyro is exactly at its calibrated zero
  /// (the offsets), so an idle mouse produces no drift.
  ImuFrames tick(int32_t dx, int32_t dy, float dt_s);

  /// Frames for an explicit angular velocity (used by the synthetic self-test
  /// sweep and handy for unit tests). Same accel handling as tick().
  ImuFrames frames_for_rates(float yaw_dps, float pitch_dps, float dt_s);

  float virtual_pitch_deg() const { return virtual_pitch_deg_; }

private:
  static int16_t clamp_i16(float v);
  void fill_accel(ImuFrame &f) const;

  Config cfg_;
  float yaw_dps_filt_ = 0.0f;
  float pitch_dps_filt_ = 0.0f;
  float virtual_pitch_deg_ = 0.0f;
};

} // namespace m2g
