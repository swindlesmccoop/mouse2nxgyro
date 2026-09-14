// Mouse -> USB-C serial for the PC relay (scripts/relay.py).
//
// Hardware: Adafruit Feather RP2040 with USB Type A Host (PIO USB host, not a
// MAX3421E wing). Mouse in the USB-A jack; USB-C to the PC.
//
// Arduino IDE:
//   Board: "Adafruit Feather RP2040 USB Host"
//   USB Stack: Adafruit TinyUSB
// Library Manager: "Adafruit TinyUSB Library"
//
// Protocol: every 8 ms while a boot-protocol mouse is mounted:
//   M <dx> <dy> <buttons>\n

#include "Adafruit_TinyUSB.h"

Adafruit_USBH_Host USBHost;

namespace {
constexpr uint32_t kBaud = 115200;
constexpr uint32_t kPeriodMs = 8;

volatile int32_t acc_dx = 0;
volatile int32_t acc_dy = 0;
volatile uint8_t buttons = 0;
volatile bool mounted = false;
} // namespace

void setup() {
  Serial.begin(kBaud);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000)
    delay(10);

  // rhport 1 = PIO USB host on this board; rhport 0 is native USB-C (Serial).
  if (!USBHost.begin(1)) {
    Serial.println("# USBHost.begin failed");
    return;
  }
  Serial.println("# mouse_relay ready (PIO USB host)");
}

void loop() {
  USBHost.task();

  static uint32_t last = 0;
  const uint32_t now = millis();
  if (now - last < kPeriodMs)
    return;
  last = now;
  if (!mounted)
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

extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const * /*desc*/, uint16_t /*len*/) {
  if (tuh_hid_interface_protocol(dev_addr, idx) != HID_ITF_PROTOCOL_MOUSE) {
    Serial.printf("# HID addr=%u idx=%u not a boot mouse, ignoring\r\n", dev_addr, idx);
    return;
  }
  mounted = true;
  acc_dx = acc_dy = 0;
  buttons = 0;
  Serial.printf("# mouse mounted addr=%u idx=%u\r\n", dev_addr, idx);
  tuh_hid_receive_report(dev_addr, idx);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  Serial.printf("# mouse unmounted addr=%u idx=%u\r\n", dev_addr, idx);
  mounted = false;
  buttons = 0;
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const *report, uint16_t len) {
  if (mounted && len >= 3) {
    buttons = report[0] & 0x1F;
    acc_dx += static_cast<int8_t>(report[1]);
    acc_dy += static_cast<int8_t>(report[2]);
  }
  tuh_hid_receive_report(dev_addr, idx);
}

} // extern "C"
