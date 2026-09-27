// Mouse -> USB-C serial for the PC relay (scripts/relay.py).
//
// Hardware: Adafruit Feather RP2040 + USB Host FeatherWing (MAX3421E).
// Mouse in the FeatherWing USB-A; this board's USB-C goes to the PC.
//
// Arduino IDE:
//   Board: "Adafruit Feather RP2040" (Earle Philhower RP2040 core)
//   USB Stack: Adafruit TinyUSB  (so Serial is USB-C CDC)
// Library Manager: "USB Host Shield Library 2.0" (felis/USB_Host_Shield_2.0)
//
// The MouseReportParser subclass follows that library's USBHIDBootMouse example.
// The library is GPL-2.0, so a binary built from this sketch is GPL-2.0 as well;
// this source file itself is MIT like the rest of the repo. See CREDITS.md.
//
// Protocol: every 8 ms, if a mouse is up, print one line (even if dx=dy=0):
//   M <dx> <dy> <buttons>\n

#include <SPI.h>
#include <Usb.h>
#include <hidboot.h>
#include <usbhub.h>

namespace {
constexpr uint32_t kBaud = 115200;
constexpr uint32_t kPeriodMs = 8;

USB usb;
HIDBoot<USB_HID_PROTOCOL_MOUSE> hid_mouse(&usb);

volatile int32_t acc_dx = 0;
volatile int32_t acc_dy = 0;
volatile uint8_t buttons = 0;

class MouseHandler : public MouseReportParser {
protected:
  void Parse(USBHID * /*hid*/, bool /*is_rpt_id*/, uint8_t len, uint8_t *buf) override {
    if (len < 3 || buf == nullptr)
      return;
    // HID boot mouse: buttons, int8 x, int8 y [, wheel]
    buttons = buf[0] & 0x1F;
    acc_dx += static_cast<int8_t>(buf[1]);
    acc_dy += static_cast<int8_t>(buf[2]);
  }
};

MouseHandler mouse_handler;
} // namespace

void setup() {
  Serial.begin(kBaud);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000)
    delay(10);

  if (usb.Init() == -1) {
    Serial.println("# usb.Init failed (MAX3421E? check FeatherWing seating / 5V)");
    return;
  }
  hid_mouse.SetReportParser(0, &mouse_handler);
  Serial.println("# mouse_relay ready (MAX3421E FeatherWing)");
}

void loop() {
  usb.Task();

  static uint32_t last = 0;
  const uint32_t now = millis();
  if (now - last < kPeriodMs)
    return;
  last = now;

  // Address 0 = no HID mouse configured. Keep sending zeros while it is up so
  // the S3 does not time out on an idle (no-report) mouse.
  if (hid_mouse.GetAddress() == 0)
    return;

  noInterrupts();
  const int32_t dx = acc_dx;
  const int32_t dy = acc_dy;
  const uint8_t btn = buttons;
  acc_dx = 0;
  acc_dy = 0;
  interrupts();

  Serial.print('M');
  Serial.print(' ');
  Serial.print(dx);
  Serial.print(' ');
  Serial.print(dy);
  Serial.print(' ');
  Serial.println(btn);
}
