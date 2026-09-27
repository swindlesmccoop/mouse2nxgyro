## Gyroscope Emulator for Switch

I will write a better readme soon. This allows you to use an RP2040 and plug in a keyboard and mouse and control games with them. The mouse emulates deltas on the gyroscope, giving more precise input than you could get from emulating the right stick. This project is built on top of a lot of other projects:
- [esp-cpp/espp](https://github.com/esp-cpp/espp)
- [TinyUSB](https://github.com/hathach/tinyusb)
- [pico-sdk](https://github.com/raspberrypi/pico-sdk)
- [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB)
- [esp_tinyusb](https://github.com/espressif/esp-usb)
- [dekuNukem](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering)
- [NXIC](https://github.com/S-thogo/NXIC)
- [SwitchProconGyroMouse](https://github.com/Bokuchin/SwitchProconGyroMouse)

More in [`CREDITS.md`](CREDITS.md).

## Hardware Required
- USB Mouse
- USB Keyboard
- [StarTech USB 2.0 Powered Hub](https://www.amazon.com/StarTech-com-Port-Compact-Black-USB/dp/B000T9S4CI) - this is the only one I got to work after testing four different hubs; many wouldn't enumerate at all
- Nintendo Switch - theoretically should work for the Switch 2 as well, but I don't have one
  - You MUST enable Wired Pro Controller Communication

## AI Disclosure
A large portion of this was written with the assistance of Fable 5.1. All features and functionality on the master branch were tested and verified by hand. Some of the experimental features on other branches (Bluetooth, ESP32, etc) are here for documentation and most likely do not work as intended.
