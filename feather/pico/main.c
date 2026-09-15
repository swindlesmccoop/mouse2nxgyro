// Mouse + keyboard host (PIO USB-A, including an unpowered hub) -> CDC lines
// on USB-C for scripts/relay.py.
//
// Dual-role TinyUSB + Pico-PIO-USB, same shape as
// Pico-PIO-USB examples/host_hid_to_device_cdc (MIT).
// Board: Adafruit Feather RP2040 with USB Type A Host
//   USB-A D+/D- = GPIO16/17, 5V boost enable = GPIO18.

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "class/hid/hid.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

#define PIN_USB_HOST_DP 16
#define PIN_5V_EN 18
#define PERIOD_MS 8

static atomic_int acc_dx;
static atomic_int acc_dy;
static atomic_uint mouse_buttons;
static atomic_bool mouse_mounted;
static atomic_bool kbd_mounted;
static atomic_uint kbd_mods;
static atomic_uint kbd_key0;
static atomic_uint kbd_key1;
static atomic_uint kbd_key2;
static atomic_uint kbd_key3;
static atomic_uint kbd_key4;
static atomic_uint kbd_key5;

static uint8_t mouse_addr, mouse_inst;
static uint8_t kbd_addr, kbd_inst;
static bool mouse_slot;
static bool kbd_slot;

static void cdc_print(const char *s) {
  if (!tud_cdc_connected())
    return;
  tud_cdc_write_str(s);
  tud_cdc_write_flush();
}

void core1_main(void) {
  sleep_ms(10);
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_HOST_DP;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
  tuh_init(1);
  while (true)
    tuh_task();
}

int main(void) {
  set_sys_clock_khz(120000, true);
  sleep_ms(10);

  gpio_init(PIN_5V_EN);
  gpio_set_dir(PIN_5V_EN, GPIO_OUT);
  gpio_put(PIN_5V_EN, 1);

  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  tud_init(0);
  cdc_print("# hid_relay ready (PIO USB host, hub+mouse+keyboard)\r\n");

  absolute_time_t next = get_absolute_time();
  while (true) {
    tud_task();
    tud_cdc_write_flush();

    if (absolute_time_diff_us(get_absolute_time(), next) > 0)
      continue;
    next = delayed_by_ms(get_absolute_time(), PERIOD_MS);
    if (!tud_cdc_connected())
      continue;

    if (atomic_load(&mouse_mounted)) {
      const int dx = atomic_exchange(&acc_dx, 0);
      const int dy = atomic_exchange(&acc_dy, 0);
      const unsigned btn = atomic_load(&mouse_buttons);
      char line[48];
      const int n = snprintf(line, sizeof(line), "M %d %d %u\n", dx, dy, btn);
      if (n > 0)
        tud_cdc_write(line, (uint32_t)n);
    }
    if (atomic_load(&kbd_mounted)) {
      char line[64];
      const int n = snprintf(line, sizeof(line), "K %u %u %u %u %u %u %u\n",
                             (unsigned)atomic_load(&kbd_mods), (unsigned)atomic_load(&kbd_key0),
                             (unsigned)atomic_load(&kbd_key1), (unsigned)atomic_load(&kbd_key2),
                             (unsigned)atomic_load(&kbd_key3), (unsigned)atomic_load(&kbd_key4),
                             (unsigned)atomic_load(&kbd_key5));
      if (n > 0)
        tud_cdc_write(line, (uint32_t)n);
    }
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report,
                      uint16_t desc_len) {
  (void)desc_report;
  (void)desc_len;
  const uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);
  tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);

  if (proto == HID_ITF_PROTOCOL_MOUSE) {
    mouse_addr = dev_addr;
    mouse_inst = instance;
    mouse_slot = true;
    atomic_store(&mouse_mounted, true);
    atomic_store(&acc_dx, 0);
    atomic_store(&acc_dy, 0);
    atomic_store(&mouse_buttons, 0);
    cdc_print("# mouse mounted\r\n");
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }
  if (proto == HID_ITF_PROTOCOL_KEYBOARD) {
    kbd_addr = dev_addr;
    kbd_inst = instance;
    kbd_slot = true;
    atomic_store(&kbd_mounted, true);
    atomic_store(&kbd_mods, 0);
    atomic_store(&kbd_key0, 0);
    atomic_store(&kbd_key1, 0);
    atomic_store(&kbd_key2, 0);
    atomic_store(&kbd_key3, 0);
    atomic_store(&kbd_key4, 0);
    atomic_store(&kbd_key5, 0);
    cdc_print("# keyboard mounted\r\n");
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }
  cdc_print("# HID not boot mouse/keyboard, ignoring\r\n");
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  if (mouse_slot && dev_addr == mouse_addr && instance == mouse_inst) {
    mouse_slot = false;
    atomic_store(&mouse_mounted, false);
    atomic_store(&mouse_buttons, 0);
    cdc_print("# mouse unmounted\r\n");
  }
  if (kbd_slot && dev_addr == kbd_addr && instance == kbd_inst) {
    kbd_slot = false;
    atomic_store(&kbd_mounted, false);
    atomic_store(&kbd_mods, 0);
    atomic_store(&kbd_key0, 0);
    atomic_store(&kbd_key1, 0);
    atomic_store(&kbd_key2, 0);
    atomic_store(&kbd_key3, 0);
    atomic_store(&kbd_key4, 0);
    atomic_store(&kbd_key5, 0);
    cdc_print("# keyboard unmounted\r\n");
  }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report,
                                uint16_t len) {
  if (mouse_slot && dev_addr == mouse_addr && instance == mouse_inst && len >= 3) {
    atomic_store(&mouse_buttons, report[0] & 0x1Fu);
    atomic_fetch_add(&acc_dx, (int)(int8_t)report[1]);
    atomic_fetch_add(&acc_dy, (int)(int8_t)report[2]);
  } else if (kbd_slot && dev_addr == kbd_addr && instance == kbd_inst && len >= 8) {
    atomic_store(&kbd_mods, report[0]);
    atomic_store(&kbd_key0, report[2]);
    atomic_store(&kbd_key1, report[3]);
    atomic_store(&kbd_key2, report[4]);
    atomic_store(&kbd_key3, report[5]);
    atomic_store(&kbd_key4, report[6]);
    atomic_store(&kbd_key5, report[7]);
  }
  tuh_hid_receive_report(dev_addr, instance);
}

void tud_cdc_rx_cb(uint8_t itf) {
  (void)itf;
  uint8_t buf[64];
  tud_cdc_read(buf, sizeof(buf));
}
