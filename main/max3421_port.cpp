// MAX3421E <-> ESP-IDF SPI/GPIO glue for TinyUSB's hcd_max3421.c.
//
// Adapted from TinyUSB's ESP32 board support package,
//   hw/bsp/espressif/boards/family.c  (functions max3421_init,
//   max3421_isr_handler, max3421_intr_task, tuh_max3421_int_api,
//   tuh_max3421_spi_cs_api, tuh_max3421_spi_xfer_api)
// hathach/tinyusb, commit 7049c58a0e895acc92c6407574b05b5536eddfc8 (v0.21.0),
// MIT License, Copyright (c) 2019 Ha Thach (tinyusb.org).
// Changes: C++ / namespaced, board pins from board_pins.hpp, optional hardware
// RESET pulse, tolerate an already-installed GPIO ISR service, SPI3 host.
// See CREDITS.md.

#include "max3421_port.hpp"

#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "tusb.h"

#include "board_pins.hpp"

namespace {

const char *TAG = "max3421";

spi_device_handle_t spi_dev = nullptr;
SemaphoreHandle_t intr_sem = nullptr;

// GPIO ISR: just wake the interrupt task. hcd_max3421's interrupt handler talks
// to the chip over SPI, which cannot happen inside a GPIO ISR.
void IRAM_ATTR isr_handler(void * /*arg*/) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(intr_sem, &woken);
  if (woken)
    portYIELD_FROM_ISR();
}

void intr_task(void * /*param*/) {
  for (;;) {
    xSemaphoreTake(intr_sem, portMAX_DELAY);
    tuh_int_handler(m2g::max3421::rhport, false);
  }
}

} // namespace

namespace m2g::max3421 {

void init() {
  using namespace board::pins;

  // Optional hardware reset. hcd_max3421.c notes: "driver does not seem to work
  // without nRST pin signal", so wire it if the breakout exposes it.
  if (max_reset != GPIO_NUM_NC) {
    gpio_reset_pin(max_reset);
    gpio_set_direction(max_reset, GPIO_MODE_OUTPUT);
    gpio_set_level(max_reset, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(max_reset, 1);
    vTaskDelay(pdMS_TO_TICKS(5)); // let the oscillator start before the first SPI access
  }

  // Chip select: manual, idle high.
  gpio_reset_pin(max_cs);
  gpio_set_direction(max_cs, GPIO_MODE_OUTPUT);
  gpio_set_level(max_cs, 1);

  // SPI bus + device. Mode 0, full duplex. 20 MHz is what TinyUSB's BSP uses on
  // the S3 (the chip itself is rated to 26 MHz).
  spi_bus_config_t buscfg{};
  buscfg.miso_io_num = max_miso;
  buscfg.mosi_io_num = max_mosi;
  buscfg.sclk_io_num = max_sck;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.data4_io_num = -1;
  buscfg.data5_io_num = -1;
  buscfg.data6_io_num = -1;
  buscfg.data7_io_num = -1;
  buscfg.max_transfer_sz = 1024;
  ESP_ERROR_CHECK(spi_bus_initialize(max_spi_host, &buscfg, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t devcfg{};
  devcfg.mode = 0;
  devcfg.clock_speed_hz = 20 * 1000 * 1000;
  devcfg.spics_io_num = -1; // CS driven by tuh_max3421_spi_cs_api
  devcfg.queue_size = 1;
  ESP_ERROR_CHECK(spi_bus_add_device(max_spi_host, &devcfg, &spi_dev));

  // Interrupt: negative edge on INT, serviced by a high-priority task.
  intr_sem = xSemaphoreCreateBinary();
  configASSERT(intr_sem);
  xTaskCreate(intr_task, "max3421_intr", 3072, nullptr, configMAX_PRIORITIES - 2, nullptr);

  gpio_reset_pin(max_int);
  gpio_set_direction(max_int, GPIO_MODE_INPUT);
  gpio_set_pull_mode(max_int, GPIO_PULLUP_ONLY);
  gpio_set_intr_type(max_int, GPIO_INTR_NEGEDGE);

  esp_err_t err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) // INVALID_STATE: already installed
    ESP_ERROR_CHECK(err);
  ESP_ERROR_CHECK(gpio_isr_handler_add(max_int, isr_handler, nullptr));
  gpio_intr_disable(max_int); // TinyUSB enables it when the chip is configured

  ESP_LOGI(TAG, "SPI%d SCK=%d MOSI=%d MISO=%d CS=%d INT=%d RST=%d", static_cast<int>(max_spi_host) + 1,
           max_sck, max_mosi, max_miso, max_cs, max_int, max_reset);
}

} // namespace m2g::max3421

//--------------------------------------------------------------------+
// API required by hcd_max3421.c (declared in tusb.h via hcd_max3421.h)
//--------------------------------------------------------------------+
extern "C" {

void tuh_max3421_int_api(uint8_t /*rhport*/, bool enabled) {
  if (enabled)
    gpio_intr_enable(board::pins::max_int);
  else
    gpio_intr_disable(board::pins::max_int);
}

void tuh_max3421_spi_cs_api(uint8_t /*rhport*/, bool active) {
  gpio_set_level(board::pins::max_cs, active ? 0 : 1);
}

bool tuh_max3421_spi_xfer_api(uint8_t /*rhport*/, uint8_t const *tx_buf, uint8_t *rx_buf,
                              size_t xfer_bytes) {
  if (tx_buf == nullptr) {
    // FIFO read: clock out whatever is in rx_buf as dummy bytes.
    tx_buf = rx_buf;
  }
  const size_t len_bits = xfer_bytes * 8;

  spi_transaction_t xact{};
  xact.length = len_bits;
  xact.rxlength = rx_buf ? len_bits : 0;
  xact.tx_buffer = tx_buf;
  xact.rx_buffer = rx_buf;

  esp_err_t err = spi_device_transmit(spi_dev, &xact);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_device_transmit: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

} // extern "C"
