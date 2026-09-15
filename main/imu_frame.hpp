#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace m2g {

/// One 6-axis IMU sample as the Switch expects it inside a 0x30 input report:
/// three int16 accelerometer axes followed by three int16 gyroscope axes, little
/// endian, RAW sensor units (the Switch applies the SPI-flash calibration we
/// serve it). Layout per dekuNukem/Nintendo_Switch_Reverse_Engineering,
/// imu_sensor_notes.md: acc_x, acc_y, acc_z, gyro_1, gyro_2, gyro_3.
struct ImuFrame {
  std::array<int16_t, 3> acc{0, 0, 0};
  std::array<int16_t, 3> gyro{0, 0, 0};
};

/// A 0x30 report carries three frames sampled 5 ms apart (15 ms per report).
using ImuFrames = std::array<ImuFrame, 3>;

/// Axis indices into ImuFrame::gyro / ImuFrame::acc, in the controller's body
/// frame. Convention taken from SDL's SDL_hidapi_switch.c remap (which converts
/// Switch raw axes to SDL's right/up/toward-player frame):
///   SDL pitch (about +X right)        = -Switch gyro[1]
///   SDL yaw   (about +Y up)           =  Switch gyro[2]
///   SDL roll  (about +Z toward player) = -Switch gyro[0]
/// so, with the controller lying flat, facing the screen: X points forward
/// (away from the player), Y points left, Z points up; gravity is +1 G on Z.
enum Axis : size_t { X = 0, Y = 1, Z = 2 };

} // namespace m2g
