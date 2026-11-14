# Cardputer ADV target

Confirmed by Alex: Cardputer Adv Version (ESP32-S3), M5Stack K132-Adv.
The Amazon short URL did not resolve; the technical source is M5Stack:
https://docs.m5stack.com/en/core/Cardputer-Adv

- Stamp-S3A / ESP32-S3FN8, dual-core Xtensa LX7 up to 240 MHz, 8 MB flash.
- ST7789V2 LCD, 240 × 135 pixels, 1.14 inches.
- 56 keys, 4 × 14; TCA8418 controller on I²C.
- ES8311 audio codec; NS4150B amplifier, 1 W speaker, 3.5 mm output.
- MEMS microphone, BMI270 IMU, microSD, Grove and 14-pin EXT header.
- 84 × 54 × 19.6 mm.
- Do not assume external PSRAM. The official specification lists flash but
  does not advertise external RAM; measure free internal RAM on the device.
- Battery documentation changed: the current official page says 1500 mAh;
  an earlier fetched version said 1750 mAh. Confirm the delivered unit label.

Audio uses GPIO 8/9 for codec control and 41/46/43/42 for serial audio.
Keyboard and IMU share the internal I²C bus. External SPI shares pins with
microSD, so additional displays must be planned with bus ownership in mind.
Use M5Stack drivers first rather than duplicating board initialization.

The native editor's display panel has the same aspect ratio; it is a layout
study, not a pixel-accurate framebuffer or a hardware timing emulator.
