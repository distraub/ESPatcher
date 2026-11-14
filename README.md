# ESPatcher

By Alex Matheu.

ESPatcher is an instrument firmware loader for Mac, iPhone/iPad and AUv3.
**AMSynth** is the custom synthesizer firmware family, with Analog and FM instruments.
AMSynth is the default reference firmware for learning to build instruments.
GlitchSynth is reserved for a future experimental instrument derived from AMSynth.
The native host loads `.gsm5` WebAssembly instruments in isolated wasm3 runtimes.
AMSynth also compiles natively for Cardputer ADV; hardware timing remains untested.
This is not an ESP32 emulator or a loader for arbitrary device firmware binaries.

## Play on Mac

Open `build/ESPatcher.app`, or open `ESPatcher.xcodeproj`, choose
`ESPatcherMac` and run on My Mac. Audio starts on launch.

- Choose **AMSynth · Analog** or **AMSynth · FM** from the Instrument menu.
- **Firmware… → Load custom firmware…** opens a custom `.gsm5` file. The two supplied files are in
  `Firmware/Modules/` and use exactly the same loader as imported modules.
- Hold the on-screen notes or click the panel and use **A W S E D F T G Y H U J K**.
- **Z / X** changes octave; **Space / Panic** clears notes.
- Shape selects sine/saw/square on Analog and FM ratios 1/2/3 on Bell.
- Attack, Release, Cutoff and Volume stay live and retain their values on switching.
- Switching drops held notes and uses a short transition ramp. Failed imports
  leave the prior instrument intact. Trapped modules become silent until reloaded.

The app and AU extension bundle the interpreter's MIT license; see
`Vendor/wasm3/README.md` for source and local execution-budget patches.

## Build

`project.yml` is authoritative. Run `xcodegen generate` after target changes.
`ESPatcherMac` embeds a macOS AUv3; `ESPatcherIOS` embeds an iPhone/iPad AUv3.
Select your signing team to run on physical iOS devices. Bundled modules are
checked in, so ordinary app builds do not need the WASI SDK or downloads.

```sh
xcodebuild -project ESPatcher.xcodeproj -scheme ESPatcherMac -destination 'platform=macOS' -derivedDataPath build/DerivedData SDK_STAT_CACHE_ENABLE=NO build
sh Tools/test.sh
```

To rebuild module files from source:

```sh
WASI_SDK_PATH=/path/to/wasi-sdk python3 Tools/build_firmware.py --fixtures
```

The tests cover module audio, native/WASM parity at 44.1/48 kHz, isolated instances,
fault handling, invalid files, slot switching, immediate notes, and AU state restore.
Apple AU validation and platform build results are recorded with each development
pass. Third-party DAW editor and physical iOS/ADV testing are still pending.

## Hardware and format

- `docs/HARDWARE.md`: verified Cardputer ADV specs.
- `docs/GSM5-FORMAT.md`: implemented module ABI, limits, switching and build guide.
- `docs/FIRMWARE-HOST.md`: original architecture notes (superseded by the format
  document where they describe the loader as future work).

AU fullState embeds the selected module and parameters. It does not preserve
held notes/tails; standalone sessions are not automatically saved yet. The
256 KiB WASM budget is for host previews, not an established ADV interpreter
budget. The native ADV target is now in `Device/ADV`; physical boot, audio, latency and CPU performance remain untested.

## Original browser study

The separate Web Audio sketch remains available: run
`python3 -m http.server 8765 --bind 127.0.0.1` and open http://127.0.0.1:8765.
It does not use the module runtime and is retained as an early sound/UI study.


## Cardputer ADV firmware

The first native hardware candidate includes both instruments, keyboard notes,
parameter editing, instrument switching and diagnostics. No microSD is needed.
See [Device/ADV/README.md](Device/ADV/README.md) for controls, build/upload steps,
recovery and the first-device test checklist. The complete first-install image is
`build/cardputer-adv/0.10.1/amsynth-adv-0.10.1-merged.bin` (flash at 0x0).
The package includes separate images, source, manifest and SHA256 checksums.
It has been cross-compiled and checked offline, **not tested on a physical ADV**.

```sh
pio run -d Device/ADV -t package
```

This target runs the same C instrument sources natively at 32 kHz. It does not
load `.gsm5` files on the ADV yet; those remain the Mac/iOS preview format.


## Four-voice instruments

Pocket Analog and FM Bell module version 0.2.0 provide four voices with per-note
attack/release and filter state, quiet-release/oldest-held voice stealing, fixed
mix headroom and a soft output peak bound. The native ADV candidate is 0.4.0.
The current Mac app uses these modules and displays active voices; the original
browser sketch remains monophonic. Existing custom mono modules still load.

On ADV press **8** to run/stop a repeating four-note test chord. Enter opens
CPU/voice diagnostics. Hardware voice capacity is unmeasured; four voices is our
starting point, with a target of staying comfortably below the 8 ms block budget.


## Ladder resonance, themes and knobs

Current instrument modules are version 0.3.0. Both use a four-pole ladder adapted
from GlitchDrums with a new Resonance control, shared coefficient smoothing and
per-voice integrators. AU address 5 appends resonance; addresses 0–4 stay stable.
Legacy five-parameter modules and states still load with resonance unavailable or
zero. New modules advertise the six-parameter `synth-v2` profile.

The Mac/iOS editor ports GlitchDrums' StudioKnobFace/StudioKnobIndicator and the
vertical-drag behavior: logarithmic cutoff/envelope controls, double-tap reset,
VoiceOver adjustment, and live AU previews with a single editor commit on release.
The existing browser study is unchanged.

All 38 FM1 themes are shared from `Assets/themes.json`, with Desert Dusk as default.
Use the app's Theme menu or ADV keys 9/0. App selection is a local UI preference;
ADV selection is currently RAM-only. See `docs/LADDER-AND-UI.md` for the adaptation
and verification details.

### LFO update

The native Mac/iOS/AUv3 editor now uses all eight knobs: cutoff, resonance, waveform, attack, release, volume, LFO rate, LFO depth. Triangle cutoff modulation is shared across voices and free-running. Rate 0.05–20 Hz; depth 0–100%, default zero. Modules are version 0.4.0 (`synth-v3`); native ADV firmware is 0.5.0. Older module profiles and AU states remain supported. Hardware CPU headroom is unmeasured.

### Performance screen

The Mac/iOS/AUv3 screen draws a live output oscilloscope and numbered 2×4 knob labels and values, including instrument-provided names (WAVE vs RATIO). The display preview is enlarged to match the knob plate height in a wider app layout while preserving its 240×135 proportions. Scope drawing is isolated at 30 fps; the native ADV 0.6.0 screen draws at up to 20 fps. Modules 0.4.1 include optional validated `knob_labels`. The audio thread writes only fixed-size atomic telemetry; display allocation and drawing stay outside audio.

### Navigation update

Native ADV firmware 0.7.0 and the Mac/iOS editor now have Home, Global, OSC and LFO pages with remembered selections, numbered page-dependent control banks, and virtual keyboard knobs. Use Command+1–4 on Mac (Fn+1–4 on ADV). Hold 1–8 plus arrows to edit, or use Tab and arrows for one-handed editing. Shift adjusts finely. Global includes an explicit-confirm instrument browser and theme preview/cancel/save; theme selection is persisted. See Device/ADV/README.md for full controls. The DSP modules remain 0.4.1, and hardware timing/physical key behavior remain untested.

### Direct screen keys (ADV 0.7.1)

C = Home, V = Global, B = OSC, N = LFO on both Mac and ADV. Command+1–4 (Mac) and Fn+1–4 (ADV) remain alternate shortcuts. Minus is reserved for Play/Stop and equals for Record arm; neither starts transport or recording yet. M/Q/I/O are reserved for future Arp/Sequence/Effects/Info screens. Number keys and arrow-marked punctuation retain virtual-knob editing.

### Arpeggiator update

M opens the Arp page; minus controls Play/Stop on Mac and ADV. Pattern, rate, gate,
octave range, latch and transport are assigned to slots 1–6. Playback is sample-clocked
in the shared C engine; the screen displays real generated-note/step telemetry.
Default is stopped, Up, 4 steps/sec, 50% gate, one octave, latch off. Rate is
free-running; tempo sync and recording are not implemented yet. Modules are 0.5.0
(synth-v4); native ADV package is 0.8.0. Legacy modules and parameter states remain
supported; older modules show inactive Arp controls. See Device/ADV/README.md for
latch, stop and panic behavior. Hardware performance remains untested.

### Tempo sync update

Global → Tempo now edits 40–300 BPM (arrows ±1, Shift ±0.1). New arp modules use
musical rate divisions, including triplets, defaulting to 1/8 at 120 BPM. Timing
stays in the audio engine and BPM changes retain step phase. Native ADV is 0.9.0,
modules 0.6.0 (synth-v5). Legacy arp modules keep their free-running Hz controls.
This is internal Global tempo, not external host/MIDI-clock synchronization.

### Animated LFO update

N now shows the FM1-style LFO reference shape, depth-scaled trace and audio-driven
phase cursor instead of the output scope. Sine, triangle, square, saw and sample/hold
are available. LFO controls are Shape / Rate / Depth / Sync. Sync follows Global BPM;
Free uses Hz. Home keeps rate/depth on 7/8. Default is half-note triangle, depth zero.
Modules 0.7.0 (synth-v6), native ADV 0.10.0. There is one shared four-voice LFO;
external clock sync and additional LFO instances remain future work.

### Loader and firmware naming

ESPatcher names the native loader application and AUv3. AMSynth names our
instrument firmware, including AMSynth Analog (`pocket_analog`) and AMSynth FM
(`fm_bell`). Existing bundle identifiers, AU component codes, parameter addresses,
state keys, `.gsm5` format and module IDs stay unchanged for compatibility.
The checkout folder remains `glitchsynth-m5`; use `ESPatcher.xcodeproj` and the
`ESPatcherMac` / `ESPatcherIOS` schemes. See docs/NAME-CHECK.md for the public-name search.

### Designing a custom engine

See [the firmware authoring guide](docs/FIRMWARE-AUTHORING.md) for engine vs preset
ownership, current module limits, real-time rules, testing expectations, and the
proposed generic page/parameter/preset contract. Proposed APIs are explicitly marked
and are not yet accepted by the loader.

## Start developing

Begin with [Getting started](docs/GETTING-STARTED.md), then read the
[authoring contract](docs/FIRMWARE-AUTHORING.md) and [module format](docs/GSM5-FORMAT.md).
AMSynth Analog and FM share the readable reference engine in `Firmware/instrument.c`.
See [the platform roadmap](docs/ROADMAP.md) for implemented features and future APIs.
Contributions: [CONTRIBUTING.md](CONTRIBUTING.md). Original code is
[MIT licensed](LICENSE); dependency notices are in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
