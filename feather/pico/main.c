// Mouse host (PIO USB-A) -> CDC lines on USB-C for scripts/relay.py.
//
// Dual-role TinyUSB + Pico-PIO-USB, same shape as
// Pico-PIO-USB examples/host_hid_to_device_cdc (MIT).
// Board: Adafruit Feather RP2040 with USB Type A Host
//   USB-A D+/D- = GPIO16/17, 5V boost enable = GPIO18.

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

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
static atomic_uint buttons;
static atomic_bool mounted;

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
  // PIO USB needs a 12 MHz multiple. 125 MHz default is not.
  set_sys_clock_khz(120000, true);
  sleep_ms(10);

  gpio_init(PIN_5V_EN);
  gpio_set_dir(PIN_5V_EN, GPIO_OUT);
  gpio_put(PIN_5V_EN, 1);

  multicore_reset_core1();
  multicore_launch_core1(core1_main);

  tud_init(0);
  cdc_print("# mouse_relay ready (PIO USB host)\r\n");

  absolute_time_t next = get_absolute_time();
  while (true) {
    tud_task();
    tud_cdc_write_flush();

    if (absolute_time_diff_us(get_absolute_time(), next) > 0)
      continue;
    next = delayed_by_ms(get_absolute_time(), PERIOD_MS);

    if (!atomic_load(&mounted) || !tud_cdc_connected())
      continue;

    const int dx = atomic_exchange(&acc_dx, 0);
    const int dy = atomic_exchange(&acc_dy, 0);
    const unsigned btn = atomic_load(&buttons);
    char line[40];
    const int n = snprintf(line, sizeof(line), "M %d %d %u\n", dx, dy, btn);
    if (n > 0)
      tud_cdc_write(line, (uint32_t)n);
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report,
                      uint16_t desc_len) {
  (void)desc_report;
  (void)desc_len;
  if (tuh_hid_interface_protocol(dev_addr, instance) != HID_ITF_PROTOCOL_MOUSE) {
    cdc_print("# HID not a boot mouse, ignoring\r\n");
    return;
  }
  atomic_store(&mounted, true);
  atomic_store(&acc_dx, 0);
  atomic_store(&acc_dy, 0);
  atomic_store(&buttons, 0);
  cdc_print("# mouse mounted\r\n");
  tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  (void)dev_addr;
  (void)instance;
  atomic_store(&mounted, false);
  atomic_store(&buttons, 0);
  cdc_print("# mouse unmounted\r\n");
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report,
                                uint16_t len) {
  if (atomic_load(&mounted) && len >= 3) {
    atomic_store(&buttons, report[0] & 0x1Fu);
    atomic_fetch_add(&acc_dx, (int)(int8_t)report[1]);
    atomic_fetch_add(&acc_dy, (int)(int8_t)report[2]);
  }
  tuh_hid_receive_report(dev_addr, instance);
}

void tud_cdc_rx_cb(uint8_t itf) {
  (void)itf;
  uint8_t buf[64];
  tud_cdc_read(buf, sizeof(buf));
}
