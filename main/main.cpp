// mouse2nxgyro (relay branch): USB mouse -> Feather -> PC serial copy -> S3 UART
// -> gyro -> Pro Controller on native USB-OTG (USB_DEV).
//
// USB device wiring, TX queue and sender-task structure follow espp's
// components/switch_pro/example/main/switch_pro_example.cpp (MIT); see CREDITS.md.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "esp_pthread.h"

#include "logger.hpp"
#include "usb_device.hpp"

#include "board.hpp"
#include "config.hpp"
#include "detail/switch_pro_spi_rom_data.hpp"
#include "gyro_switch_pro.hpp"
#include "imu_calibration.hpp"
#include "mouse_gyro_model.hpp"
#include "mouse_host.hpp"
#include "mouse_protocol.hpp"

using namespace std::chrono_literals;

namespace {

/// Build the model configuration from config.hpp and the IMU calibration the
/// Switch will read from our emulated SPI flash (user bank 0x8028..0x803F).
m2g::MouseGyroModel::Config make_model_config() {
  using namespace m2g;
  MouseGyroModel::Config c;
  c.cal = parse_imu_calibration(sp::spi_rom_data_80, 0x28);
  c.deg_per_count = config::deg_per_count;
  c.smoothing = config::smoothing;
  c.accel_mode = config::accel_mode;
  if (config::invert_y)
    c.pitch_sign = -c.pitch_sign;
  return c;
}

/// Apply mouse, keyboard-mapped pad, and board buttons to the Pro Controller report.
void apply_buttons(espp::SwitchPro::InputReport &r, uint8_t mouse_btns, bool up, bool down) {
  using namespace m2g::config;
  using namespace m2g::protocol;
  const bool grip = up && down;
  const bool live = m2g::mouse_host::pad_live();
  const uint32_t bits = live ? m2g::mouse_host::pad_buttons() : 0;

  if (live) {
    r.set_button_a((bits & btn_a) || board::button_ok());
    r.set_button_b(bits & btn_b);
    r.set_button_x(bits & btn_x);
    r.set_button_y(bits & btn_y);
    r.set_button_l((bits & btn_l) || grip);
    r.set_button_r((bits & btn_r) || grip);
    r.set_button_zl(bits & btn_zl);
    r.set_button_zr(bits & btn_zr);
    r.set_button_minus(bits & btn_minus);
    r.set_button_plus(bits & btn_plus);
    r.set_button_thumb_l(bits & btn_l3);
    r.set_button_thumb_r(bits & btn_r3);
    r.set_button_home((bits & btn_home) || board::button_menu());
    r.set_button_capture(bits & btn_capture);
    r.set_dpad((bits & btn_dpad_up) != 0, (bits & btn_dpad_down) != 0,
               (bits & btn_dpad_left) != 0, (bits & btn_dpad_right) != 0);
    r.set_left_joystick(m2g::mouse_host::stick_lx(), m2g::mouse_host::stick_ly());
    return;
  }

  r.set_button_l((mouse_btns & mouse_buttons.l) || grip);
  r.set_button_r((mouse_btns & mouse_buttons.r) || grip);
  r.set_button_zl(mouse_btns & mouse_buttons.zl);
  r.set_button_zr(mouse_btns & mouse_buttons.zr);
  r.set_button_thumb_r(mouse_btns & mouse_buttons.thumb_r);
  r.set_button_a(board::button_ok());
  r.set_button_home(board::button_menu());
}

} // namespace

extern "C" void app_main(void) {
  espp::Logger logger({.tag = "mouse2nxgyro", .level = espp::Logger::Verbosity::INFO});
  logger.info("mouse2nxgyro starting (relay: mouse via UART0 / Micro-USB)");

  // --- Board: USB PHY on USB_DEV (Pro Controller). USB_HOST VBUS is unused here.
  board::init();

  // --- Mouse side: UART lines from scripts/relay.py (Feather USB-C serial).
  m2g::mouse_host::start();

  // --- Gyro model.
  m2g::MouseGyroModel model(make_model_config());
  {
    const auto &cal = model.config().cal;
    logger.info("IMU cal from ROM: gyro offset ({}, {}, {}) sens {}, acc sens {}; {:.3f} deg/count",
                cal.gyro_offset[0], cal.gyro_offset[1], cal.gyro_offset[2], cal.gyro_sens[2],
                cal.acc_sens[2], model.config().deg_per_count);
  }

  // --- Switch side: protocol engine with our IMU path.
  m2g::GyroSwitchPro controller({.log_level = espp::Logger::Verbosity::WARN});

  // Outgoing HID reports are produced from two contexts (the TinyUSB device task,
  // when a host OUTPUT report arrives, and our periodic sender). Funnel them
  // through one queue drained by a single sender task so write_hid_report() is
  // never called from the TinyUSB task.
  struct OutReport {
    uint8_t id;
    std::vector<uint8_t> data;
  };
  std::deque<OutReport> tx_queue;
  std::mutex tx_mutex;
  std::condition_variable tx_cv;
  auto enqueue = [&](espp::SwitchPro::ReportData rd) {
    {
      std::lock_guard<std::mutex> lock(tx_mutex);
      if (tx_queue.size() < 16) // cap so a misbehaving host cannot grow it unbounded
        tx_queue.push_back({rd.first, std::move(rd.second)});
    }
    tx_cv.notify_one();
  };

  // One HID interface advertising the Pro Controller report descriptor, with an
  // interrupt-OUT endpoint for the host's OUTPUT reports (the handshake).
  // Nintendo's VID/PID are required for a real Switch to bind it (emulation only).
  espp::UsbDevice::Config usb_cfg;
  usb_cfg.vid = espp::SwitchPro::vid;
  usb_cfg.pid = espp::SwitchPro::pid;
  usb_cfg.manufacturer = espp::SwitchPro::manufacturer_name;
  usb_cfg.product = espp::SwitchPro::product_name;
  usb_cfg.log_level = espp::Logger::Verbosity::WARN;

  espp::UsbDevice::HidFunction hid;
  hid.interface_name = "Switch Pro Controller";
  hid.report_descriptor = controller.get_report_descriptor();
  hid.has_out_endpoint = true;
  hid.poll_interval_ms = 8; // the real Pro Controller's bInterval (full speed)
  hid.on_receive = [&](std::span<const uint8_t> data) {
    // TinyUSB device task context: compute the reply and queue it.
    if (data.empty())
      return;
    if (auto reply = controller.on_hid_report(data[0], data.data(), data.size()))
      enqueue(std::move(*reply));
  };
  usb_cfg.hid = hid;

  espp::UsbDevice usb(usb_cfg);
  usb.set_mount_callback([&]() {
    logger.info("USB mounted by host; starting Pro Controller handshake");
    if (auto init = controller.on_attach())
      enqueue(std::move(*init));
  });
  usb.set_unmount_callback([&]() { logger.warn("USB unmounted"); });

  std::error_code ec;
  if (!usb.initialize(ec)) {
    logger.error("Failed to initialize USB device: {}", ec.message());
    return;
  }
  logger.info("USB device ready; plug USB_DEV into the dock. Mouse: python3 scripts/relay.py");

  std::atomic<bool> running{true};

  // --- Sender task: drain queued replies first; otherwise, once the Switch has
  // enabled input reports, stream a 0x30 report every report_period_ms carrying
  // the mouse-derived IMU frames.
  {
    // std::thread -> pthread; give it a real stack and a priority above app_main
    // (1) and the TinyUSB device task (5) so report timing does not depend on
    // logging in the housekeeping loop.
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "sender";
    cfg.stack_size = 6144;
    cfg.prio = 8;
    esp_pthread_set_cfg(&cfg);
  }
  std::thread sender([&]() {
    using clock = std::chrono::steady_clock;
    auto last_tick = clock::now();
    bool was_mounted = false;

    while (running.load()) {
      OutReport rep;
      bool have = false;
      {
        std::unique_lock<std::mutex> lock(tx_mutex);
        tx_cv.wait_for(lock, std::chrono::milliseconds(m2g::config::report_period_ms),
                       [&]() { return !tx_queue.empty() || !running.load(); });
        if (!tx_queue.empty()) {
          rep = std::move(tx_queue.front());
          tx_queue.pop_front();
          have = true;
        }
      }

      if (have) {
        std::error_code send_ec;
        for (int i = 0; i < 20 && !usb.write_hid_report(rep.id, rep.data, send_ec); ++i)
          std::this_thread::sleep_for(1ms);
        continue;
      }

      if (!controller.is_ready())
        continue;

      // Measured dt since the previous streamed report, so integrated rotation is
      // exactly counts * deg_per_count whatever the actual cadence was.
      const auto now = clock::now();
      const float dt_s = std::chrono::duration<float>(now - last_tick).count();
      last_tick = now;

      const bool mounted = m2g::mouse_host::mounted();
      if (was_mounted && !mounted)
        model.reset();
      was_mounted = mounted;

      const int32_t dx = m2g::mouse_host::take_dx();
      const int32_t dy = m2g::mouse_host::take_dy();
      const uint8_t mouse_btns = m2g::mouse_host::buttons();

      const bool up = board::button_up();
      const bool down = board::button_down();

      m2g::ImuFrames frames;
      if (m2g::config::synthetic_sweep_enabled && (up != down)) {
        // Self-test: steady rotation as if the mouse were moving right (UP) or
        // toward the player (DW). See config.hpp.
        frames = model.frames_for_rates(up ? m2g::config::synthetic_dps : 0.0f,
                                        down ? m2g::config::synthetic_dps : 0.0f, dt_s);
      } else {
        frames = model.tick(dx, dy, dt_s);
      }

      controller.update_input_report([&](espp::SwitchPro::InputReport &r) {
        r.reset();
        apply_buttons(r, mouse_btns, up, down);
      });
      controller.set_imu_frames(frames);

      auto report = controller.get_input_report();
      if (!report.empty()) {
        std::error_code send_ec;
        usb.write_hid_report(controller.input_report_id(), report, send_ec); // best effort
      }
    }
  });

  // --- Housekeeping: LEDs + a status line every few seconds.
  uint32_t last_reports = 0;
  int loops = 0;
  while (true) {
    board::set_led_green(controller.is_ready());
    board::set_led_yellow(m2g::mouse_host::mounted());

    if (++loops % 50 == 0) { // every 5 s
      const uint32_t reports = m2g::mouse_host::report_count();
      logger.info("switch: {}  imu: {}  mouse: {} ({} reports/s)  over-current: {}",
                  controller.is_ready() ? "streaming" : "waiting",
                  controller.is_imu_enabled() ? "on" : "off",
                  m2g::mouse_host::mounted() ? "mounted" : "absent", (reports - last_reports) / 5,
                  board::over_current() ? "YES" : "no");
      last_reports = reports;
    }
    std::this_thread::sleep_for(100ms);
  }

  running.store(false);
  tx_cv.notify_all();
  sender.join();
}
