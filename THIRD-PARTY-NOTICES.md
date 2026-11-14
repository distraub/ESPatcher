# Third-party notices

ESPatcher's original code is MIT licensed (see LICENSE). Dependency code retains
its own copyright and license; the root license does not replace these notices.

- **wasm3 v0.5.0**: MIT. Vendored runtime and local sandbox changes are described
  in [Vendor/wasm3/README.md](Vendor/wasm3/README.md); full upstream notice:
  [Vendor/wasm3/LICENSE](Vendor/wasm3/LICENSE).
- **M5Cardputer, M5Unified and M5GFX**: device dependencies pinned in
  [Device/ADV/platformio.ini](Device/ADV/platformio.ini). PlatformIO obtains their
  upstream sources and license notices; they are not vendored in this repository.
- **Arduino-ESP32, ESP-IDF and toolchains**: obtained by PlatformIO for the device
  build; retain their applicable notices when redistributing dependency artifacts.
- **GlitchDrums control/filter designs and shared Glitch theme palettes**:
  Alex Matheu's related projects; adapted into this reference implementation.

RemoteFM1 informed platform and management-interface discussions. No RemoteFM1
source is included or relicensed by this project's MIT license.
