#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace m2g {

/// The 6-axis calibration block a Pro Controller stores in SPI flash and the
/// Switch reads during pairing (dekuNukem spi_flash_notes.md / imu_sensor_notes.md).
/// The console converts our raw int16 samples to physical units with:
///
///   gyro_dps = (raw - gyro_offset) * 936.0 / (gyro_sens - gyro_offset)
///   accel_G  =  raw * 4.0 / (acc_sens - acc_origin)
///
/// The model must therefore use the SAME numbers to go the other way, or the
/// Switch would scale our motion wrongly. They are read from the very bytes espp
/// serves in the user-calibration bank (0x8028..0x803F, preferred by the Switch
/// over the factory bank when its 0xB2 0xA1 magic is present).
struct ImuCalibration {
  std::array<int16_t, 3> acc_origin{};
  std::array<int16_t, 3> acc_sens{};
  std::array<int16_t, 3> gyro_offset{};
  std::array<int16_t, 3> gyro_sens{};

  /// Raw gyro value for `dps` degrees per second on `axis`.
  constexpr float gyro_raw_from_dps(size_t axis, float dps) const {
    return static_cast<float>(gyro_offset[axis]) +
           dps * static_cast<float>(gyro_sens[axis] - gyro_offset[axis]) / 936.0f;
  }
  /// Raw accel value for `g` G on `axis`.
  constexpr float acc_raw_from_g(size_t axis, float g) const {
    return g * static_cast<float>(acc_sens[axis] - acc_origin[axis]) / 4.0f;
  }
};

/// Parse a 24-byte IMU calibration block (acc_origin[3], acc_sens[3],
/// gyro_offset[3], gyro_sens[3], all int16 little endian).
template <typename ByteArray>
constexpr ImuCalibration parse_imu_calibration(const ByteArray &rom, size_t offset) {
  auto rd = [&](size_t at) -> int16_t {
    return static_cast<int16_t>(static_cast<uint16_t>(rom[at]) |
                                (static_cast<uint16_t>(rom[at + 1]) << 8));
  };
  ImuCalibration c{};
  for (size_t i = 0; i < 3; ++i) {
    c.acc_origin[i] = rd(offset + 0 + 2 * i);
    c.acc_sens[i] = rd(offset + 6 + 2 * i);
    c.gyro_offset[i] = rd(offset + 12 + 2 * i);
    c.gyro_sens[i] = rd(offset + 18 + 2 * i);
  }
  return c;
}

} // namespace m2g
