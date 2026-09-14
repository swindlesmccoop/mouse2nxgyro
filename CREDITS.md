# Credits and third-party notices

mouse2nxgyro is MIT licensed (see `LICENSE`). It stands on the projects below.
Whole components are consumed as git submodules with their license files intact;
anything adapted into this repository's own sources carries an inline comment at
the point of use pointing back here.

## Code we build against (submodules under `external/espp`)

| Project | What we use | License | Pinned at |
| --- | --- | --- | --- |
| [esp-cpp/espp](https://github.com/esp-cpp/espp) | `switch_pro` (Pro Controller protocol engine), `usb_device` (TinyUSB device wrapper), `hid-rp` (report structs/descriptor), `logger`, `task`, `timer`, `format`, `base_component` | MIT, © 2022 esp-cpp | `038eea4a63d3bc4c287b6c1420924a646abb5ee2` |
| [hathach/tinyusb](https://github.com/hathach/tinyusb) (via [espressif/tinyusb](https://github.com/espressif/tinyusb), vendored by espp at `external/tinyusb`) | Device stack (through esp_tinyusb) **and** host stack: `src/host/usbh.c`, `src/class/hid/hid_host.c`, `src/portable/analog/max3421/hcd_max3421.c`, compiled by `components/tinyusb_host` | MIT, © 2012–2026 hathach (tinyusb.org) | `7049c58a0e895acc92c6407574b05b5536eddfc8` (v0.21.0) |
| [espressif/esp-usb](https://github.com/espressif/esp-usb) `device/esp_tinyusb` (vendored by espp at `external/esp-usb`) | ESP-IDF integration of the TinyUSB device stack; owns `tusb_config.h` | Apache-2.0 (individual files MIT) | `2d0900896c78dade98374b97eb09eda1d3bb8a82` (esp_tinyusb 2.2.1) |
| [intergatedcircuits/hid-rp](https://github.com/intergatedcircuits/hid-rp) (via espp `components/hid-rp/detail/hid-rp`) | Compile-time HID report descriptor library | MPL-2.0 | `d64ec1db212bbe2cdf427cefd82657355b2ad739` |
| [fmtlib/fmt](https://github.com/fmtlib/fmt) (via espp `components/format/detail/fmt`) | Formatting, used by espp's logger | MIT-style ({fmt} license) | `93e26fa578712891ee0a5302e7563e0ba79efef9` |
| [Espressif ESP-IDF](https://github.com/espressif/esp-idf) v6.0.1 | SDK, toolchain, FreeRTOS, drivers (via the `espressif/idf:v6.0.1` Docker image) | Apache-2.0 | image digest in `Dockerfile` |

## Code adapted into this repository

- **`main/max3421_port.cpp`** — SPI transfer, chip-select, interrupt-enable
  callbacks and the GPIO-ISR-to-task pattern are adapted from TinyUSB's ESP32 board
  support package, `hw/bsp/espressif/boards/family.c` (functions `max3421_init`,
  `max3421_isr_handler`, `max3421_intr_task`, `tuh_max3421_int_api`,
  `tuh_max3421_spi_cs_api`, `tuh_max3421_spi_xfer_api`), hathach/tinyusb commit
  `7049c58`, MIT, © 2019 Ha Thach. Changes: C++, board pins from `board_pins.hpp`,
  optional hardware RESET pulse, tolerate an already-installed GPIO ISR service.
- **`main/main.cpp`** — USB device configuration, TX queue and sender-task
  structure follow espp's `components/switch_pro/example/main/switch_pro_example.cpp`
  (MIT, © 2022 esp-cpp).
- **`sdkconfig.defaults`** — derived from espp's
  `components/switch_pro/example/sdkconfig.defaults`.

## Protocol and IMU references (no code copied)

- **[dekuNukem/Nintendo_Switch_Reverse_Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering)**
  — the foundational Pro Controller/Joy-Con protocol documentation. Everything about
  report layout, subcommands, SPI-flash calibration and the IMU unit conversions
  (`imu_sensor_notes.md`: `dps = (raw - offset) * 936 / (sens - offset)`,
  `G = raw * 4 / (acc_sens - acc_origin)`) traces back here, directly or through espp.
- **[Brikwerk/nxbt](https://github.com/Brikwerk/nxbt)** (MIT) — espp's protocol
  reference for the emulated controller state machine.
- **[libsdl-org/SDL](https://github.com/libsdl-org/SDL) `src/joystick/hidapi/SDL_hidapi_switch.c`**
  (zlib) — used to pin down the raw-axis convention (gyro Z = yaw, gyro Y = pitch,
  negated; at rest accel = (0, 0, +1 G)). Convention only, no code.

## Prior art for mouse-to-gyro (concept and conventions)

- **[S-thogo/NXIC](https://github.com/S-thogo/NXIC)** and its fork
  **[mizuyoukanao/NXIC](https://github.com/mizuyoukanao/NXIC)** (MIT) — PC-side
  mouse-to-gyro for the Switch. We adopt its axis/sign mapping (mouse X → −gyro Z,
  mouse Y → +gyro Y, identical value in all three IMU frames) and its button
  convention (left click = ZR, right click = R). Its `raw = (delta / threshold) *
  57.3 / 0.070` constant-scale approach informed ours; we replace it with a measured-dt
  conversion through the served calibration so rotation is independent of mouse poll
  rate.
- **[Bokuchin/SwitchProconGyroMouse](https://github.com/Bokuchin/SwitchProconGyroMouse)**
  (no license stated) — proof of concept that adds scaled mouse deltas to a real
  Pro Controller's gyro stream via a USB man-in-the-middle. Concept credit only; no
  code was used or consulted for implementation.

## Hardware documentation

- Espressif, *ESP32-S3-USB-OTG User Guide* — pin map, USB switch circuit, VBUS
  power options (`main/board_pins.hpp`).
- Espressif, *ESP32-S3 Series Datasheet* — strapping-pin and GPIO restrictions
  behind the MAX3421E pin assignment.
- Analog Devices / Maxim, *MAX3421E datasheet* — SPI protocol (implemented by
  TinyUSB's `hcd_max3421.c`).

Nintendo, Nintendo Switch and Pro Controller are trademarks of Nintendo. This is an
independent, unaffiliated hobby project; the Nintendo USB VID/PID are used only so
the console will bind the emulated controller.
