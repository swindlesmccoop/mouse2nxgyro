#include "mouse_host.hpp"

#include "mouse_protocol.hpp"

#include <array>
#include <atomic>
#include <cstdio>

#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

const char *TAG = "mouse_uart";

std::atomic<int32_t> acc_dx{0};
std::atomic<int32_t> acc_dy{0};
std::atomic<int32_t> acc_wheel{0};
std::atomic<uint8_t> btn_mask{0};
std::atomic<bool> is_mounted{false};
std::atomic<uint32_t> reports{0};
std::atomic<int64_t> last_m_us{0};
std::atomic<int64_t> last_c_us{0};
std::atomic<uint32_t> raw_lines{0};
std::atomic<int32_t> lx_milli{0};
std::atomic<int32_t> ly_milli{0};
std::atomic<uint32_t> pad_bits{0};
std::atomic<bool> pad_is_live{false};

constexpr uart_port_t kUart = UART_NUM_0;
constexpr int kLineMax = 96;

void apply_line(const char *line) {
  raw_lines.fetch_add(1);
  if (raw_lines.load() <= 8) {
    ESP_LOGI(TAG, "uart line: '%s'", line);
  }

  const char *s = line;
  while (*s == ' ' || *s == '\t')
    ++s;
  const int64_t now = esp_timer_get_time();

  if (*s == m2g::protocol::mouse_prefix) {
    int dx = 0, dy = 0, buttons = 0;
    const int n = std::sscanf(s, "M %d %d %d", &dx, &dy, &buttons);
    if (n < 2)
      return;
    acc_dx.fetch_add(dx);
    acc_dy.fetch_add(dy);
    if (n >= 3)
      btn_mask.store(static_cast<uint8_t>(buttons & 0x1F));
    reports.fetch_add(1);
    last_m_us.store(now);
    is_mounted.store(true);
    return;
  }

  if (*s == m2g::protocol::pad_prefix) {
    int lx = 0, ly = 0;
    unsigned bits = 0;
    if (std::sscanf(s, "C %d %d %u", &lx, &ly, &bits) < 3)
      return;
    if (lx > 1000)
      lx = 1000;
    if (lx < -1000)
      lx = -1000;
    if (ly > 1000)
      ly = 1000;
    if (ly < -1000)
      ly = -1000;
    lx_milli.store(lx);
    ly_milli.store(ly);
    pad_bits.store(bits);
    last_c_us.store(now);
    pad_is_live.store(true);
    is_mounted.store(true);
  }
}

void rx_task(void * /*param*/) {
  std::array<char, kLineMax> line{};
  size_t len = 0;

  for (;;) {
    uint8_t byte = 0;
    const int n = uart_read_bytes(kUart, &byte, 1, pdMS_TO_TICKS(20));
    const int64_t now = esp_timer_get_time();
    const int64_t stale = static_cast<int64_t>(m2g::protocol::stale_ms) * 1000;
    if (is_mounted.load() && (now - last_m_us.load()) > stale && (now - last_c_us.load()) > stale) {
      is_mounted.store(false);
      btn_mask.store(0);
    }
    if (pad_is_live.load() && (now - last_c_us.load()) > stale) {
      pad_is_live.store(false);
      lx_milli.store(0);
      ly_milli.store(0);
      pad_bits.store(0);
    }
    if (n != 1)
      continue;

    if (byte == '\r')
      continue;
    if (byte == '\n') {
      if (len > 0) {
        line[len] = '\0';
        apply_line(line.data());
        len = 0;
      }
      continue;
    }
    if (len + 1 < line.size())
      line[len++] = static_cast<char>(byte);
    else
      len = 0;
  }
}

} // namespace

namespace m2g::mouse_host {

void start() {
  fflush(stdout);
  fsync(fileno(stdout));

  if (uart_is_driver_installed(kUart)) {
    uart_driver_delete(kUart);
  }

  uart_config_t cfg = {};
  cfg.baud_rate = static_cast<int>(m2g::protocol::baud);
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  ESP_ERROR_CHECK(uart_param_config(kUart, &cfg));
  const esp_err_t err = uart_driver_install(kUart, 4096, 0, 0, nullptr, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_driver_install(UART0): %s — mouse relay will not work",
             esp_err_to_name(err));
    return;
  }
  uart_vfs_dev_use_driver(kUart);
  uart_flush_input(kUart);

  xTaskCreate(rx_task, "mouse_uart", 3072, nullptr, 6, nullptr);
  ESP_LOGI(TAG, "listening for M/C packets on UART0 @ %u baud. ./scripts/dev.sh relay",
           m2g::protocol::baud);
}

bool mounted() { return is_mounted.load(); }
uint8_t buttons() { return btn_mask.load(); }
int32_t take_dx() { return acc_dx.exchange(0); }
int32_t take_dy() { return acc_dy.exchange(0); }
int32_t take_wheel() { return acc_wheel.exchange(0); }
uint32_t report_count() { return reports.load(); }
bool pad_live() { return pad_is_live.load(); }
float stick_lx() { return static_cast<float>(lx_milli.load()) / 1000.0f; }
float stick_ly() { return static_cast<float>(ly_milli.load()) / 1000.0f; }
uint32_t pad_buttons() { return pad_bits.load(); }

} // namespace m2g::mouse_host
