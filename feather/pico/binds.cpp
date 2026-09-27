#include "binds.hpp"

#include "mouse_protocol.hpp"

#include <cmath>

namespace {

// USB HID boot-protocol usage IDs used by config/binds.json.
constexpr uint8_t kW = 0x1A;
constexpr uint8_t kA = 0x04;
constexpr uint8_t kS = 0x16;
constexpr uint8_t kD = 0x07;
constexpr uint8_t kE = 0x08;
constexpr uint8_t kQ = 0x14;
constexpr uint8_t kR = 0x15;
constexpr uint8_t kF = 0x09;
constexpr uint8_t kSpace = 0x2C;
constexpr uint8_t kEsc = 0x29;
constexpr uint8_t kTab = 0x2B;
constexpr uint8_t kHome = 0x4A;
constexpr uint8_t kMinus = 0x2D;
constexpr uint8_t kPrintScreen = 0x46;
constexpr uint8_t kY = 0x1C;
constexpr uint8_t kRight = 0x4F;
constexpr uint8_t kLeft = 0x50;
constexpr uint8_t kDown = 0x51;
constexpr uint8_t kUp = 0x52;

constexpr uint8_t kModLShift = 0x02;

constexpr uint8_t kMouseLeft = 1u << 0;
constexpr uint8_t kMouseRight = 1u << 1;
constexpr uint8_t kMouseMiddle = 1u << 2;
constexpr uint8_t kMouseBack = 1u << 3;
constexpr uint8_t kMouseForward = 1u << 4;

bool usage_held(uint8_t usage, const uint8_t keys[6]) {
  for (int i = 0; i < 6; ++i) {
    if (keys[i] == usage)
      return true;
  }
  return false;
}

} // namespace

void binds_map(uint8_t mods, const uint8_t keys[6], uint8_t mouse_btns, float *lx, float *ly,
               uint32_t *pad_bits) {
  const bool up = usage_held(kW, keys);
  const bool down = usage_held(kS, keys);
  const bool left = usage_held(kA, keys);
  const bool right = usage_held(kD, keys);

  float x = (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f);
  float y = (up ? 1.0f : 0.0f) - (down ? 1.0f : 0.0f);
  const float mag = std::hypot(x, y);
  if (mag > 1.0f) {
    x /= mag;
    y /= mag;
  }
  *lx = x;
  *ly = y;

  using namespace m2g::protocol;
  uint32_t bits = 0;
  if (usage_held(kSpace, keys))
    bits |= btn_b;
  if (mods & kModLShift)
    bits |= btn_zl;
  if (usage_held(kE, keys))
    bits |= btn_r;
  if (usage_held(kQ, keys))
    bits |= btn_x;
  if (usage_held(kR, keys))
    bits |= btn_plus;
  if (usage_held(kF, keys))
    bits |= btn_a;
  if (usage_held(kEsc, keys) || usage_held(kHome, keys))
    bits |= btn_home;
  if (usage_held(kTab, keys))
    bits |= btn_r3;
  if (usage_held(kMinus, keys))
    bits |= btn_minus;
  if (usage_held(kPrintScreen, keys))
    bits |= btn_capture;
  if (usage_held(kY, keys))
    bits |= btn_y;
  if (usage_held(kUp, keys))
    bits |= btn_dpad_up;
  if (usage_held(kDown, keys))
    bits |= btn_dpad_down;
  if (usage_held(kLeft, keys))
    bits |= btn_dpad_left;
  if (usage_held(kRight, keys))
    bits |= btn_dpad_right;

  if (mouse_btns & kMouseLeft)
    bits |= btn_zr;
  if (mouse_btns & kMouseRight)
    bits |= btn_zl;
  if (mouse_btns & kMouseForward)
    bits |= btn_l;
  if (mouse_btns & kMouseBack)
    bits |= btn_r;
  if (mouse_btns & kMouseMiddle)
    bits |= btn_r3;

  *pad_bits = bits;
}
