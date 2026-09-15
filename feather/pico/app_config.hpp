#pragma once

#include "mouse_gyro_model.hpp"

namespace app {

inline constexpr int report_period_ms = 15;
inline constexpr float deg_per_count = 0.1f;
inline constexpr bool invert_y = false;
inline constexpr float smoothing = 0.0f;
// 1 G "flat" makes games pull look to the horizon. Tilting that vector with
// the mouse (TrackedPitch) makes them pull to the sky/floor instead. 0 G
// skips that correction so gyro look can hold.
inline constexpr m2g::MouseGyroModel::AccelMode accel_mode =
    m2g::MouseGyroModel::AccelMode::NoCorrection;

} // namespace app
