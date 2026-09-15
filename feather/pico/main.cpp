// Dual-role: USB-C = wired Switch Pro Controller; USB-A (PIO) = hub + mouse + keyboard.
// Board: Adafruit Feather RP2040 with USB Type A Host (D+ GPIO16, 5V boost GPIO18).

#include <atomic>
#include <cstring>

#include "app_config.hpp"
#include "binds.hpp"
#include "class/hid/hid.h"
#include "detail/switch_controller_protocol.hpp"
#include "detail/switch_pro_spi_rom_data.hpp"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "imu_calibration.hpp"
#include "mouse_gyro_model.hpp"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pio_usb.h"
#include "switch_pro_pico.hpp"
#include "tusb.h"

#define PIN_USB_HOST_DP 16
#define PIN_5V_EN 18

static std::atomic<int> acc_dx{0};
static std::atomic<int> acc_dy{0};
static std::atomic<unsigned> mouse_buttons{0};
static std::atomic<bool> mouse_mounted{false};
static std::atomic<bool> kbd_mounted{false};
static std::atomic<unsigned> kbd_mods{0};
static std::atomic<unsigned> kbd_key0{0};
static std::atomic<unsigned> kbd_key1{0};
static std::atomic<unsigned> kbd_key2{0};
static std::atomic<unsigned> kbd_key3{0};
static std::atomic<unsigned> kbd_key4{0};
static std::atomic<unsigned> kbd_key5{0};

static uint8_t mouse_addr, mouse_inst;
static uint8_t kbd_addr, kbd_inst;
static bool mouse_slot;
static bool kbd_slot;

static PicoSwitchPro g_pad;
static bool g_need_attach = true;
static HidReply g_held_reply{};

void core1_main(void) {
  sleep_ms(10);
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_HOST_DP;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
  tuh_init(1);
  while (true)
    tuh_task();
}

static void send_hid(uint8_t id, const uint8_t *data63) {
  if (!tud_hid_ready())
    return;
  tud_hid_report(id, data63, 63);
}

int main(void) {
  set_sys_clock_khz(120000, true);
  sleep_ms(10);

  gpio_init(PIN_5V_EN);
  gpio_set_dir(PIN_5V_EN, GPIO_OUT);
  gpio_put(PIN_5V_EN, 1);

  m2g::MouseGyroModel::Config gcfg{};
  gcfg.cal = m2g::parse_imu_calibration(sp::spi_rom_data_80, 0x28);
  gcfg.deg_per_count = app::deg_per_count;
  gcfg.smoothing = app::smoothing;
  gcfg.accel_mode = app::accel_mode;
  m2g::MouseGyroModel model(gcfg);

  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  tud_init(0);

  absolute_time_t next = get_absolute_time();
  absolute_time_t last_tick = get_absolute_time();
  bool was_mounted = false;

  while (true) {
    tud_task();

    if (g_need_attach && tud_mounted()) {
      g_pad.on_attach();
      g_need_attach = false;
    }

    if (!g_held_reply.valid)
      g_pad.take_reply(&g_held_reply);
    if (g_held_reply.valid && tud_hid_ready()) {
      send_hid(g_held_reply.report_id, g_held_reply.data);
      g_held_reply.valid = false;
    }

    if (absolute_time_diff_us(get_absolute_time(), next) > 0)
      continue;
    next = delayed_by_ms(get_absolute_time(), app::report_period_ms);

    if (!g_pad.hid_ready() || !tud_hid_ready())
      continue;

    const absolute_time_t now = get_absolute_time();
    float dt_s = static_cast<float>(absolute_time_diff_us(last_tick, now)) / 1e6f;
    last_tick = now;

    const bool mounted = mouse_mounted.load();
    if (was_mounted && !mounted)
      model.reset();
    was_mounted = mounted;

    int32_t dx = acc_dx.exchange(0);
    int32_t dy = acc_dy.exchange(0);
    if (app::invert_y)
      dy = -dy;

    uint8_t keys[6] = {
        static_cast<uint8_t>(kbd_key0.load()), static_cast<uint8_t>(kbd_key1.load()),
        static_cast<uint8_t>(kbd_key2.load()), static_cast<uint8_t>(kbd_key3.load()),
        static_cast<uint8_t>(kbd_key4.load()), static_cast<uint8_t>(kbd_key5.load()),
    };
    float lx = 0, ly = 0;
    uint32_t pad = 0;
    binds_map(static_cast<uint8_t>(kbd_mods.load()), keys,
              static_cast<uint8_t>(mouse_buttons.load()), &lx, &ly, &pad);

    const m2g::ImuFrames frames = model.tick(dx, dy, dt_s);
    g_pad.set_pad(lx, ly, pad);
    g_pad.set_imu(frames);
    g_pad.increment_counter();

    uint8_t report[63];
    g_pad.fill_standard_report(report);
    send_hid(sp::DEVICE_INPUT_REPORT, report);
  }
}

extern "C" void tud_mount_cb(void) { g_need_attach = true; }

extern "C" void tud_umount_cb(void) { g_need_attach = true; }

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                          hid_report_type_t report_type, uint8_t *buffer,
                                          uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type, uint8_t const *buffer,
                                      uint16_t bufsize) {
  (void)instance;
  (void)report_type;
  g_pad.on_hid_out(report_id, buffer, bufsize);
}

extern "C" void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report,
                                 uint16_t desc_len) {
  (void)desc_report;
  (void)desc_len;
  const uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);
  tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);

  if (proto == HID_ITF_PROTOCOL_MOUSE) {
    mouse_addr = dev_addr;
    mouse_inst = instance;
    mouse_slot = true;
    mouse_mounted.store(true);
    acc_dx.store(0);
    acc_dy.store(0);
    mouse_buttons.store(0);
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }
  if (proto == HID_ITF_PROTOCOL_KEYBOARD) {
    kbd_addr = dev_addr;
    kbd_inst = instance;
    kbd_slot = true;
    kbd_mounted.store(true);
    kbd_mods.store(0);
    kbd_key0.store(0);
    kbd_key1.store(0);
    kbd_key2.store(0);
    kbd_key3.store(0);
    kbd_key4.store(0);
    kbd_key5.store(0);
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }
}

extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  if (mouse_slot && dev_addr == mouse_addr && instance == mouse_inst) {
    mouse_slot = false;
    mouse_mounted.store(false);
    mouse_buttons.store(0);
  }
  if (kbd_slot && dev_addr == kbd_addr && instance == kbd_inst) {
    kbd_slot = false;
    kbd_mounted.store(false);
    kbd_mods.store(0);
    kbd_key0.store(0);
    kbd_key1.store(0);
    kbd_key2.store(0);
    kbd_key3.store(0);
    kbd_key4.store(0);
    kbd_key5.store(0);
  }
}

extern "C" void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                           uint8_t const *report, uint16_t len) {
  if (mouse_slot && dev_addr == mouse_addr && instance == mouse_inst && len >= 3) {
    mouse_buttons.store(report[0] & 0x1Fu);
    acc_dx.fetch_add((int)(int8_t)report[1]);
    acc_dy.fetch_add((int)(int8_t)report[2]);
  } else if (kbd_slot && dev_addr == kbd_addr && instance == kbd_inst && len >= 8) {
    kbd_mods.store(report[0]);
    kbd_key0.store(report[2]);
    kbd_key1.store(report[3]);
    kbd_key2.store(report[4]);
    kbd_key3.store(report[5]);
    kbd_key4.store(report[6]);
    kbd_key5.store(report[7]);
  }
  tuh_hid_receive_report(dev_addr, instance);
}
