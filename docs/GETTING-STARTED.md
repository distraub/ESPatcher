# Build your first ESPatcher instrument

ESPatcher is the Mac/iOS/AUv3 loader. AMSynth is its default reference firmware.
AMSynth Analog (`pocket_analog`) and FM (`fm_bell`) are separate portable engines
built from the same C source. GlitchSynth is a future instrument, not a current
module. Existing IDs remain stable through the AMSynth rename.

## Browser route (no Mac required)

Serve the repository with `python3 -m http.server 8765 --bind 127.0.0.1`, then
open http://127.0.0.1:8765 and enable audio. Windows and Linux users can play the
same bundled WASM engines without Xcode. The browser host has the pad grid,
inline knobs, chord mode, screen navigation, tempo and themes. Custom module
importing is not enabled; see the browser section in the root README for limits.
AudioWorklet requires HTTPS or localhost. Run `node Tests/web-runtime.cjs` to
verify the browser audio path without a browser UI.

## 1. Run the reference instrument

On macOS, install Xcode with its command-line tools and XcodeGen. From the repo:

```sh
xcodegen generate
xcodebuild -project ESPatcher.xcodeproj -scheme ESPatcherMac \
  -destination 'platform=macOS' -derivedDataPath build/DerivedData \
  SDK_STAT_CACHE_ENABLE=NO build
open build/DerivedData/Build/Products/Debug/ESPatcher.app
```

Bundled modules are checked in; the Apple build does not need a WASI SDK.
For iOS, choose `ESPatcherIOS` in Xcode. To run on your iPad or iPhone:

1. Connect and unlock the device, trust the Mac, and enable Developer Mode if prompted.
2. Add your Apple developer account in Xcode Settings → Accounts.
3. Create `Apple/Signing.local.xcconfig` containing `DEVELOPMENT_TEAM = YOUR_TEAM_ID`.
   This file is ignored by Git and applies to both the app and AUv3 extension.
4. Select the device and run the `ESPatcherIOS` scheme. Automatic signing creates
   the development provisioning profiles. Alternatively select the same team in
   Signing & Capabilities for both `ESPatcherIOS` and `ESPatcherIOSAU`.

`Apple/Signing.xcconfig` optionally includes the local file. Device builds use
Apple Development signing; Mac and Simulator builds retain ad-hoc signing.
Use Firmware… to switch engines or load a custom `.gsm5` module.

## 2. Follow one control through the source

- `Firmware/instrument.c`: note handling, voice allocation, render and parameters.
- `Firmware/firmware.h`: exported ABI functions.
- `Firmware/ladder.h`, `lfo.h`, `arp.h`: reusable synthesis components.
- `Tools/build_firmware.py`: variants, stable IDs and module manifests.
- `Apple/Shared/SynthParameter.swift`: current editor parameter definitions.
- `Core/Navigation.hpp`: shared screen/knob routing for the current profile.
- `Device/ADV/src/main.cpp`: physical keyboard, display and audio integration.

Start by changing the reference engine's sound without changing its parameter
profile. Preserve its real-time and state-lifetime rules. Read
[FIRMWARE-AUTHORING.md](FIRMWARE-AUTHORING.md) before extending the interface.

## 3. Rebuild and test the modules

Install a WASI SDK for your development machine and set its path:

```sh
WASI_SDK_PATH=/path/to/wasi-sdk python3 Tools/build_firmware.py --fixtures
sh Tools/test.sh
```

The build produces `Firmware/Modules/pocket_analog.gsm5` and `fm_bell.gsm5`.
Load one in ESPatcher to audition it. Keep your own engine ID distinct when
creating a new instrument; update the build manifest and `gs_id` consistently.
Do not redistribute a changed instrument under AMSynth's existing IDs.

The current build helper targets these two reference variants. A standalone
third-party starter template and arbitrary parameter/page schemas are future work.
[GSM5-FORMAT.md](GSM5-FORMAT.md) documents the current supported export contract.

## 4. Build for Cardputer ADV

Install PlatformIO, then:

```sh
pio run -d Device/ADV -t package
```

Follow [the device guide](../Device/ADV/README.md) before flashing.
The device builds the C engine natively; it does not run arbitrary ESP32 binaries
inside the Mac app or currently load WASM from microSD. Physical audio, CPU,
latency and peripheral behavior remain unverified until tested on a real ADV.

## Current performance controls

C/V/B/N/M select Home/Global/OSC/LFO/Arp. Hold 1–8 and use arrows to edit knobs.
A W S E D F T G Y H U J K play notes; Z/X change octave; Space is panic.
Minus controls arp transport. The Mac app also has a 4×4 chromatic pad grid.
Option+1–4 select major/minor/seventh/minor-seventh chord mode, then any note or
pad supplies its root. Repeating the selection turns chord mode off.
Pads and chord mode are currently Apple editor features, not implemented device
peripheral drivers or native ADV chord controls. A sequencer is not implemented.

## Prototype SD packages

Open Library in the browser to try multi-engine instruments with their own UI
scenes. See [SD-INSTRUMENT-PROTOTYPE.md](SD-INSTRUMENT-PROTOTYPE.md) for package
authoring, SD staging, the separate ADV runtime and its hardware proof checklist.
