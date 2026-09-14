#include "mouse_host.hpp"

#include <atomic>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tusb.h"

#include "max3421_port.hpp"

namespace {

const char *TAG = "mouse_host";

// Shared with the TinyUSB host task via atomics only.
std::atomic<int32_t> acc_dx{0};
std::atomic<int32_t> acc_dy{0};
std::atomic<int32_t> acc_wheel{0};
std::atomic<uint8_t> btn_mask{0};
std::atomic<bool> is_mounted{false};
std::atomic<uint32_t> reports{0};

// The one mouse interface we are polling (CFG_TUH_DEVICE_MAX = 1, but a mouse
// may expose several HID interfaces, e.g. a "gaming" mouse with a keyboard
// interface for macros; we only drive the boot-protocol mouse one).
std::atomic<int16_t> mouse_key{-1}; // (dev_addr << 8 | idx), or -1

constexpr int16_t make_key(uint8_t dev_addr, uint8_t idx) {
  return static_cast<int16_t>((dev_addr << 8) | idx);
}

void host_task(void * /*param*/) {
  // Root port 1 = MAX3421E (rhport 0 is the native controller, in device mode
  // for the Switch and owned by esp_tinyusb).
  const tusb_rhport_init_t host_init = {
      .role = TUSB_ROLE_HOST,
      .speed = TUSB_SPEED_FULL,
  };
  if (!tuh_rhport_init(m2g::max3421::rhport, &host_init)) {
    ESP_LOGE(TAG, "tuh_rhport_init failed: MAX3421E not responding? Check SPI wiring, "
                  "3V3/5V supply and the RESET line.");
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(TAG, "TinyUSB host up on rhport %u (MAX3421E)", m2g::max3421::rhport);

  for (;;) {
    // Blocks on the host event queue; returns to let us breathe.
    tuh_task_ext(/*timeout_ms=*/100, /*in_isr=*/false);
  }
}

} // namespace

namespace m2g::mouse_host {

void start() {
  max3421::init();
  // Above the TinyUSB device task (esp_tinyusb default priority 5) but below
  // the MAX3421E interrupt task, so enumeration keeps up under load.
  xTaskCreatePinnedToCore(host_task, "usbh_mouse", 4096, nullptr, 6, nullptr, tskNO_AFFINITY);
}

bool mounted() { return is_mounted.load(); }
uint8_t buttons() { return btn_mask.load(); }
int32_t take_dx() { return acc_dx.exchange(0); }
int32_t take_dy() { return acc_dy.exchange(0); }
int32_t take_wheel() { return acc_wheel.exchange(0); }
uint32_t report_count() { return reports.load(); }

} // namespace m2g::mouse_host

//--------------------------------------------------------------------+
// TinyUSB host callbacks (called from the host task)
//--------------------------------------------------------------------+
extern "C" {

void tuh_mount_cb(uint8_t daddr) {
  uint16_t vid = 0, pid = 0;
  tuh_vid_pid_get(daddr, &vid, &pid);
  ESP_LOGI(TAG, "USB device %u attached, VID:PID %04x:%04x", daddr, vid, pid);
}

void tuh_umount_cb(uint8_t daddr) { ESP_LOGI(TAG, "USB device %u detached", daddr); }

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, const uint8_t * /*report_desc*/,
                      uint16_t desc_len) {
  const uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, idx);
  ESP_LOGI(TAG, "HID interface mounted: addr=%u idx=%u itf_protocol=%u report_desc=%u bytes",
           dev_addr, idx, itf_protocol, desc_len);

  if (itf_protocol != HID_ITF_PROTOCOL_MOUSE) {
    // Either a keyboard interface on a composite device, or a mouse that is not
    // boot-protocol capable (bInterfaceSubClass != 1). v1 handles boot-protocol
    // mice only; a report-descriptor parser is a listed follow-up.
    ESP_LOGW(TAG, "  not a boot-protocol mouse interface; ignoring");
    return;
  }
  if (mouse_key.load() != -1) {
    ESP_LOGW(TAG, "  a mouse interface is already active; ignoring this one");
    return;
  }

  // TinyUSB asked for boot protocol during enumeration (CFG_TUH_HID_SET_PROTOCOL_ON_ENUM);
  // confirm it took, otherwise the byte layout below is not guaranteed.
  if (tuh_hid_get_protocol(dev_addr, idx) != HID_PROTOCOL_BOOT) {
    ESP_LOGW(TAG, "  device stayed in report protocol; parsing as boot mouse anyway "
                  "(deltas may be wrong)");
  }

  mouse_key.store(make_key(dev_addr, idx));
  acc_dx.store(0);
  acc_dy.store(0);
  acc_wheel.store(0);
  btn_mask.store(0);
  is_mounted.store(true);

  if (!tuh_hid_receive_report(dev_addr, idx)) {
    ESP_LOGE(TAG, "  tuh_hid_receive_report failed");
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  ESP_LOGI(TAG, "HID interface unmounted: addr=%u idx=%u", dev_addr, idx);
  if (mouse_key.load() == make_key(dev_addr, idx)) {
    mouse_key.store(-1);
    is_mounted.store(false);
    btn_mask.store(0);
    acc_dx.store(0);
    acc_dy.store(0);
    acc_wheel.store(0);
  }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, const uint8_t *report,
                                uint16_t len) {
  if (mouse_key.load() == make_key(dev_addr, idx) && len >= 3) {
    // HID boot-protocol mouse report: buttons, int8 x, int8 y[, int8 wheel[, int8 pan]].
    // Byte layout per the USB HID 1.11 spec, Appendix B.2 (matches TinyUSB's
    // hid_mouse_report_t).
    hid_mouse_report_t r{};
    std::memcpy(&r, report, len < sizeof(r) ? len : sizeof(r));

    btn_mask.store(r.buttons);
    acc_dx.fetch_add(r.x);
    acc_dy.fetch_add(r.y);
    if (len >= 4)
      acc_wheel.fetch_add(r.wheel);
    reports.fetch_add(1);
  }
  // Re-arm the interrupt IN transfer for the next report.
  if (!tuh_hid_receive_report(dev_addr, idx)) {
    ESP_LOGE(TAG, "tuh_hid_receive_report re-arm failed");
  }
}

} // extern "C"
