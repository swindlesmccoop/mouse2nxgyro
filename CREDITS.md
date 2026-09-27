# Credits and third-party notices

mouse2nxgyro is MIT licensed (see `LICENSE`). It stands on the projects below.
Some are git submodules with their license files intact, some are fetched by the
Docker toolchain images at pinned versions and never stored in this repository,
and some are libraries the Arduino sketches build against. Code adapted into this
repository's own sources carries an inline comment at the top of the file
pointing back here.

## Code we build against: ESP32-S3 firmware (submodules under `external/espp`)

| Project | What we use | License | Pinned at |
| --- | --- | --- | --- |
| [esp-cpp/espp](https://github.com/esp-cpp/espp) | `switch_pro` (Pro Controller protocol engine), `usb_device` (TinyUSB device wrapper), `hid-rp` (report structs/descriptor), `logger`, `task`, `timer`, `format`, `base_component` | MIT, © 2022 esp-cpp | `038eea4a63d3bc4c287b6c1420924a646abb5ee2` |
| [hathach/tinyusb](https://github.com/hathach/tinyusb) (via [espressif/tinyusb](https://github.com/espressif/tinyusb), vendored by espp at `external/tinyusb`) | Device stack (through esp_tinyusb) **and** host stack: `src/host/usbh.c`, `src/class/hid/hid_host.c`, `src/portable/analog/max3421/hcd_max3421.c`, compiled by `components/tinyusb_host` | MIT, © 2012–2026 hathach (tinyusb.org) | `7049c58a0e895acc92c6407574b05b5536eddfc8` (v0.21.0) |
| [espressif/esp-usb](https://github.com/espressif/esp-usb) `device/esp_tinyusb` (vendored by espp at `external/esp-usb`) | ESP-IDF integration of the TinyUSB device stack; owns `tusb_config.h` | Apache-2.0 (individual files MIT) | `2d0900896c78dade98374b97eb09eda1d3bb8a82` (esp_tinyusb 2.2.1) |
| [intergatedcircuits/hid-rp](https://github.com/intergatedcircuits/hid-rp) (via espp `components/hid-rp/detail/hid-rp`) | Compile-time HID report descriptor library | MPL-2.0 | `d64ec1db212bbe2cdf427cefd82657355b2ad739` |
| [fmtlib/fmt](https://github.com/fmtlib/fmt) (via espp `components/format/detail/fmt`) | Formatting, used by espp's logger | MIT-style ({fmt} license) | `93e26fa578712891ee0a5302e7563e0ba79efef9` |
| [Espressif ESP-IDF](https://github.com/espressif/esp-idf) v6.0.1 | SDK, toolchain, FreeRTOS, drivers (via the `espressif/idf:v6.0.1` Docker image) | Apache-2.0 | image digest in `Dockerfile` |

## Code we build against: Feather RP2040 firmware (`feather/pico`)

Fetched by `Dockerfile.feather` when the toolchain image is built. None of it is
stored in this repository.

| Project | What we use | License | Pinned at |
| --- | --- | --- | --- |
| [raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk) | RP2040 runtime, clocks, GPIO, multicore, build system, `adafruit_feather_rp2040_usb_host` board config | BSD-3-Clause, © 2020 Raspberry Pi (Trading) Ltd. | `bddd20f928ce76142793bef434d4f75f4af6e433` (2.1.1) |
| [hathach/tinyusb](https://github.com/hathach/tinyusb) (pico-sdk's `lib/tinyusb` submodule) | Device stack on USB-C (the Pro Controller), host stack, hub driver and HID host class on USB-A | MIT, © 2018 hathach (tinyusb.org) | `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` (0.18.0) |
| [sekigon-gonnoc/Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB) | The PIO-emulated USB host behind the USB-A port | MIT, © 2021 sekigon-gonnoc | `fe9133fc513b82cc3dc62c67cb51f2339cf29ef7` (0.6.1) |
| [esp-cpp/espp](https://github.com/esp-cpp/espp) (the same submodule) | `switch_pro` headers included directly: `detail/switch_controller_protocol.hpp` (report IDs, subcommands, device info) and `detail/switch_pro_spi_rom_data.hpp` (factory/user calibration blobs) | MIT, © 2022 esp-cpp | `038eea4a63d3bc4c287b6c1420924a646abb5ee2` |
| Debian bookworm `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib` | Cross compiler; newlib's C library and libstdc++ are linked into the UF2 | GCC: GPL-3.0 with the GCC Runtime Library Exception; newlib: BSD-style (various) | `15:12.2.rel1-1`, `3.3.0-1.3+deb12u1`, `15:12.2.rel1-1+23` |

A compiled `mouse_relay.uf2` contains code from all of the above. If you publish
UF2 files, ship these notices with them.

## Libraries the Arduino sketches build against (not built by Docker)

| Project | Used by | License |
| --- | --- | --- |
| [earlephilhower/arduino-pico](https://github.com/earlephilhower/arduino-pico) | Board support for both sketches | LGPL-2.1 (bundles pico-sdk, BSD-3-Clause) |
| [adafruit/Adafruit_TinyUSB_Arduino](https://github.com/adafruit/Adafruit_TinyUSB_Arduino) | `feather/usb_a_host/mouse_relay.ino` (PIO USB host, USB-C CDC serial) | MIT |
| [felis/USB_Host_Shield_2.0](https://github.com/felis/USB_Host_Shield_2.0) | `feather/max3421_wing/mouse_relay.ino` (MAX3421E FeatherWing) | GPL-2.0, © 2011 Circuits At Home, LTD |

The sketches' own source is MIT, but a binary built from
`feather/max3421_wing/mouse_relay.ino` links USB_Host_Shield_2.0 and is therefore
GPL-2.0.

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
- **`feather/pico/switch_pro_pico.cpp`** and **`switch_pro_pico.hpp`** — a port of
  espp's `components/switch_pro/src/switch_pro.cpp` and `src/protocol.cpp` to the
  RP2040: the 0x80 handshake, the 0x01 subcommand replies (device info, SPI-flash
  reads, IMU enable, player lights, NFC/IR), the SPI read bank logic and the
  standard 0x30 report layout. espp commit `038eea4`, MIT, © 2022 esp-cpp; espp
  itself follows [Brikwerk/nxbt](https://github.com/Brikwerk/nxbt)
  `nxbt/controller/protocol.py` (MIT). Changes: no espp task/timer/logger, a
  single pending reply, fixed serial and MAC, IMU frames from the mouse model, and
  the user stick calibration's magic bytes cleared so the Switch uses the factory
  stick calibration.
- **`feather/pico/usb_descriptors.c`** — the 203-byte Pro Controller HID report
  descriptor and the "Nintendo Co., Ltd." / "Pro Controller" strings are espp's
  (`espp::switch_pro_descriptor()`, built with hid-rp), MIT, © 2022 esp-cpp. The
  descriptor callbacks follow TinyUSB's `examples/device/*/src/usb_descriptors.c`,
  MIT, © 2019 Ha Thach.
- **`feather/pico/main.cpp`** — the dual-role startup (120 MHz system clock, PIO
  host configured and run on core1 with `PIO_USB_DEFAULT_CONFIG`, device stack on
  core0) follows Pico-PIO-USB's
  `examples/host_hid_to_device_cdc/host_hid_to_device_cdc.c` at tag 0.6.1 (MIT,
  © 2019 Ha Thach). `tusb_config.h` uses the usual TinyUSB example boilerplate.
- **`feather/max3421_wing/mouse_relay.ino`** — the `MouseReportParser` subclass
  follows USB_Host_Shield_2.0's `USBHIDBootMouse` example (GPL-2.0).

## Protocol, USB and IMU references (no code copied)

- **[dekuNukem/Nintendo_Switch_Reverse_Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering)**
  — the foundational Pro Controller/Joy-Con protocol documentation. Everything about
  report layout, subcommands, SPI-flash calibration (including the factory and user
  stick calibration layouts at 0x603D and 0x8010) and the IMU unit conversions
  (`imu_sensor_notes.md`: `dps = (raw - offset) * 936 / (sens - offset)`,
  `G = raw * 4 / (acc_sens - acc_origin)`) traces back here, directly or through espp.
- **[Brikwerk/nxbt](https://github.com/Brikwerk/nxbt)** (MIT) — espp's protocol
  reference for the emulated controller state machine.
- **[libsdl-org/SDL](https://github.com/libsdl-org/SDL) `src/joystick/hidapi/SDL_hidapi_switch.c`**
  (zlib) — used to pin down the raw-axis convention (gyro Z = yaw, gyro Y = pitch,
  negated; at rest accel = (0, 0, +1 G)). Convention only, no code.
- **USB-IF, *Device Class Definition for HID 1.11* and *HID Usage Tables*** — the
  boot keyboard and boot mouse report layouts parsed in `feather/pico/main.cpp`, and
  the keyboard usage IDs in `feather/pico/binds.cpp` and `scripts/binds.py`.
- **[hathach/tinyusb issue #2971](https://github.com/hathach/tinyusb/issues/2971)**
  — a hub problem on the same Feather board that went away on an older
  Pico-PIO-USB release, which pointed us at pinning an older version. The fix
  here was 0.6.1.
- **[qmk/qmk_firmware](https://github.com/qmk/qmk_firmware)** docs and
  `tmk_core/protocol/report.h` (GPL-2.0) — read while debugging how a QMK keyboard
  reports keys in boot vs report protocol. Nothing from it is in the code.

## Prior art for mouse-to-gyro (concept and conventions)

- **[S-thogo/NXIC](https://github.com/S-thogo/NXIC)** and its fork
  **[mizuyoukanao/NXIC](https://github.com/mizuyoukanao/NXIC)** (MIT) — PC-side
  mouse-to-gyro for the Switch. We adopt its axis/sign mapping (mouse X → −gyro Z,
  mouse Y → +gyro Y, identical value in all three IMU frames) and left click = ZR.
  Its `raw = (delta / threshold) * 57.3 / 0.070` constant-scale approach informed
  ours; we replace it with a measured-dt conversion through the served calibration
  so rotation is independent of mouse poll rate.
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
- Adafruit, [*Feather RP2040 with USB Type A Host*](https://learn.adafruit.com/adafruit-feather-rp2040-with-usb-type-a-host)
  guide — USB host D+/D− on GPIO16/17, 5 V boost enable on GPIO18, red LED on
  GPIO13 (`feather/pico/main.cpp`).

Nintendo, Nintendo Switch and Pro Controller are trademarks of Nintendo. This is an
independent, unaffiliated hobby project; the Nintendo USB VID/PID are used only so
the console will bind the emulated controller.
