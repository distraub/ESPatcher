# AMSynth Cardputer ADV — 0.10.1 hardware candidate

By Alex Matheu. Target: **M5Stack Cardputer Adv Version (ESP32-S3), K132-Adv**.
This image is compiled and packaged, but has not been booted on physical hardware.
No microSD, PSRAM, Wi-Fi, Bluetooth, external controls, or computer is needed to play.

## What it contains

- Pocket Analog and FM Bell, compiled natively from `Firmware/instrument.c`, the
  same portable C source used by the Mac's `.gsm5` modules.
- Four voices with independent envelopes, four-pole ladder filter state and release tails.
- Idle slot, quiet release tail, then oldest-held-note allocation. Stealing uses
  a 32-sample transition; output has fixed 6 dB headroom and a soft peak bound.
- 32 kHz audio, 256-frame blocks, ES8311 codec through M5Unified's speaker driver.
- Built-in 240 × 135 display and ADV TCA8418 keyboard through official drivers.
- Audio producer/playback run on core 1; keyboard/display run on core 0.
- Fixed input queue and three static PCM buffers.
  Playback release callbacks control buffer reuse; UI/serial work stays outside DSP.
- Instrument changes have a 128-sample transition. Panic and octave changes drop
  held notes until their keys are released, preventing accidental stuck/retriggered notes.

This build does **not** interpret `.gsm5` files on-device, persist sound edits, sample
audio, or provide MIDI. Both examples are built into the flash image. Loading
custom native instruments currently means rebuilding this target. An ADV adapter
and native instrument execution are provided here; a hardware WASM runtime is
still an experiment for later.

## Controls

| Key | Action |
|---|---|
| A W S E D F T G Y H U J K | Chromatic C–C notes |
| Fn + 1 / 2 / 3 / 4 | Home / Global / OSC / LFO |
| Hold 1–8 + arrows | Turn that page's virtual knob |
| Shift + adjustment | Fine edit |
| Tab / Shift+Tab | Enter one-handed editing, next / previous knob |
| Left / right in knob-edit mode | Select knob |
| Up / down in knob-edit mode | Edit selected knob |
| Arrows in Global | Browse items and preview themes |
| OK / Enter | Open or confirm; keep theme |
| Esc / Backspace | Cancel preview, leave editing, or return Home |
| Z / X | Octave down / up, range 1–7 |
| Space or side G0 button | Panic; release held keys before playing again |
| Fn + 8 | Toggle diagnostics |

The arrow-marked punctuation keys work without Fn: comma = left, slash = right,
semicolon = up, period = down. Home starts at cutoff. Shape selects sine/saw/square
on Analog and 1:1/2:1/3:1 ratios on Bell. Parameters are retained across instrument
changes, lost on reboot. Theme is saved explicitly from Global and restored on boot.
Resonance ranges from 0–100%; its top end enters self-oscillation while a voice is
active. Cutoff tops out at 14.4 kHz on the 32 kHz ADV build. Edits preserve filter
state. The speaker driver master volume is fixed at 160/255; instrument Level
ranges from 0 to .5 (displayed as 0–100%).
Start with a low Level when testing headphones. Inserting a headphone plug disables
the built-in speaker on the ADV.

## Build and package

Install PlatformIO Core in a Python virtual environment, then run from the repo:

```sh
pio run -d Device/ADV
pio run -d Device/ADV -t package
```

The platform is pinned to `espressif32@6.12.0`, Arduino-ESP32 2.0.17, generic
ESP32-S3 N8 / 8 MB QD flash / no PSRAM. M5Cardputer, M5Unified and M5GFX are
pinned to exact official Git commits in platformio.ini. The driver package
licenses remain with their source dependencies; original project sources are
included in `source.tar.gz`. The app's `project.yml` does not build this target.

Outputs are in `build/cardputer-adv/0.10.1/` at the repo root:

- `amsynth-adv-0.10.1-merged.bin`: first-install image, flash at **0x0**.
- `firmware.bin`: app only, flash at **0x10000**, only with this package's layout.
- `bootloader.bin`, `partitions.bin`, `boot_app0.bin`: component images.
- `manifest.json`, `SHA256SUMS`, `source.tar.gz`: identity and reproducible source.

Packaging verifies each component at its exact offset in the merged image. The
partition layout provides two 3 MiB app slots and reserves the remainder for
storage; this firmware does not implement OTA updates or use that storage yet.

## When the device arrives

First flashing replaces the currently installed firmware. The merged image also
overwrites the NVS area with erased bytes, so back up any settings you want first.
Use a USB-C **data** cable. Choose the Cardputer's serial port explicitly; never
assume a port name in a script. To list ports:

```sh
pio device list
```

M5Stack's ADV download-mode instructions: switch power OFF; hold G0 while
connecting USB-C to the computer, then release G0 after power is applied.
https://docs.m5stack.com/en/core/Cardputer-Adv

From source, this uploads the bootloader, table and app to their correct offsets:

```sh
pio run -d Device/ADV -t upload --upload-port /dev/cu.YOUR_CARDPUTER_PORT
pio device monitor -b 115200 --port /dev/cu.YOUR_CARDPUTER_PORT
```

Or use esptool v4 with the merged image (replace PORT, no connected device has
been flashed as part of preparing this package):

```sh
python -m esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x0 amsynth-adv-0.10.1-merged.bin
```

Reset or power-cycle after flashing. The screen should show Pocket Analog.
If it reports “ADV not detected” or audio startup failure, keep the serial log;
do not force an original Cardputer pin map. To return to factory firmware,
use M5Stack's official M5Burner and select the Cardputer-ADV factory image:
https://docs.m5stack.com/en/uiflow/m5burner/intro

## First-device checks

1. Confirm the screen orientation, all note presses/releases, equal-count key
   swaps, octave changes, panic and both instruments.
2. Check speaker and headphone output separately at low volume; note whether
   unplugging headphones returns sound to the speaker.
3. Hold a note while changing parameters and switching instruments; check for
   stuck notes, unexpected full-volume output, clicks and dropouts.
4. Fn+8 opens diagnostics. `DSP max` should stay below the 8000 µs block budget.
   Press **8** for the repeating four-note chord, including note-on costs.
   `Voices` counts sounding voices including release tails. `Late` counts
   synthesis/conversion times over that budget. `Q gap` counts an
   empty software playback queue (includes startup); it is **not** a measured
   audible-underrun count. Free heap includes runtime allocations, unlike the
   static RAM figure printed by the linker. Serial prints these every two seconds.
5. Play and edit for several minutes, check heap stability and queue overflow.
   Send the serial output if anything fails. CPU capacity, actual codec rate,
   note latency, battery behavior and buffer continuity require the device.

Control logic can be checked on the Mac without the board:

```sh
clang++ -std=c++17 -O2 Device/ADV/tests/controls.cpp -o /tmp/gs-adv-controls
/tmp/gs-adv-controls
```


## Polyphony and CPU limits

Four voices is the initial engineering target, not a measured maximum for ADV.
Only active voices run DSP. Sine/FM use a 1024-entry interpolated sine table
initialized once at boot; saw/square use float PolyBLEP. Sample-loop phase math
is float, suited to ESP32-S3's single-precision FPU. Block coefficients are shared.
Voices and tables use fixed storage, with no heap allocation during rendering.

The shared source supports `GS_VOICE_COUNT` from 1 to 8 for native experiments;
this package is fixed to four. Do not infer eight-voice reliability from the switch.
CPU time, codec output and keyboard rollover still require the physical board.
FM Bell remains a simple FM example and can alias at high pitches/ratios.


## Ladder filter and themes

The ladder follows GlitchDrums' GDLadderFilter: four TPT stages with a linear
zero-delay feedback solve and stage saturation, feedback up to 4.2. The ESP32
adaptation uses a 257-point lookup for a bounded tanh approximation and shared
smoothed coefficients. It is close in character, not a bit-identical copy of the
full app filter. Four voices remain enabled; actual DSP time needs hardware.

`Assets/themes.json` contains the original FM1 RGB565 palette values and stable
38-theme order. Regenerate C/Swift palettes with `python3 Tools/generate_themes.py`.
The default is Desert Dusk, index 24. Theme changes do not generate audio commands.

## LFO controls

The eight-control order is cutoff, resonance, shape, attack, release, volume, LFO rate, LFO depth. Tab advances in this order on Home; up/down edit. The shared shape-selectable LFO modulates cutoff. Free rate is 0.05–20 Hz; depth 0–100% corresponds to up to ±3 octaves, clamped to the filter range. Depth defaults to zero. No physical timing measurements have been taken.

## Performance screen

The normal 240×135 screen now shows the instrument identity, voice count, octave, output oscilloscope and a numbered 2×4 control map. Selected controls are highlighted; FM Bell calls slot 3 RATIO while Pocket Analog calls it WAVE. Fn+8 toggles diagnostics and the normal view; keyboard controls remain the same. A 240×135 RGB565 sprite is allocated once (64,800 bytes of heap), then pushed at up to 20 fps. Audio publishes to a lock-free scope ring independently of drawing. This framebuffer and timing still require validation on the physical device.

## Screen navigation and virtual knobs (0.10.1)

- Fn+1 Home, Fn+2 Global, Fn+3 OSC, Fn+4 LFO. Fn+8 toggles diagnostics.
- Home: cutoff, resonance, shape, attack, release, level, LFO rate, LFO depth.
- OSC: shape, attack, release, cutoff, resonance, level; slots 7–8 inactive.
- LFO: rate, depth; slots 3–8 inactive. LFO remains triangle → cutoff.
- Global: Instrument and Theme browsers. Arrows move, OK opens/confirms. Instrument browsing does not load until OK.
- Hold 1–8 and use arrow-marked keys to change that page's knob. Shift makes adjustments ten times finer; held arrows repeat after 400 ms and accelerate after two seconds.
- Tab enters one-handed editing and advances slots (Shift+Tab reverses); left/right choose a slot, up/down edit it. Esc leaves editing, then returns Home. In Global, Tab moves browser entries instead.
- Arrow-marked physical keys work without Fn: comma = left, slash = right, semicolon = up, period = down. SDK arrow events are accepted too.
- Esc/backspace cancels theme preview or returns Home. OK saves theme; NVS write waits until keys and audio tails are quiet. Boot restores the saved palette. Navigation positions remain session-local.
- Existing letter note keys, Z/X octave and Space/BtnA panic remain available across screens. Keys 1/2 no longer load instruments directly; 8 is now a virtual knob selector, not the chord test. Test chord remains in the Mac app.
- Global keeps Home knob assignments; browsing and theme changes do not reset audio. Loading a different instrument clears held notes safely.

The Mac preview uses Command+1–4 or the navigation buttons instead of Fn+1–4. All virtual-knob/browser behavior uses the same `Core/Navigation.hpp` state machine through an Objective-C++ bridge. A generic descriptor for arbitrary instrument pages and external-knob pickup are future work; these four pages target the current supported synth profiles.

### Direct screen keys (ADV 0.10.1)

C = Home, V = Global, B = OSC, N = LFO on both Mac and ADV. Command+1–4 (Mac) and Fn+1–4 (ADV) remain alternate shortcuts. Minus is reserved for Play/Stop and equals for Record arm; neither starts transport or recording yet. M/Q/I/O are reserved for future Arp/Sequence/Effects/Info screens. Number keys and arrow-marked punctuation retain virtual-knob editing.

## Arpeggiator (0.10.1)

M opens Arp; minus toggles Play/Stop once per press, regardless of keyboard repeat.
Fn+5 is an alternate Arp shortcut. Equals remains reserved for recording.
The Arp bank is: 1 Pattern, 2 Rate, 3 Gate, 4 Octaves, 5 Latch, 6 Play, 7/8 unused.
Patterns are Up, Down, Up/Down (no repeated endpoints), and deterministic Random.
Rate uses Global tempo and musical divisions, with gate 5–100% and octave range 1–4.
Defaults: stopped, Up, 1/8 at 120 BPM, 50% gate, one octave, latch off.

When stopped, new notes play normally. Play switches the currently held chord into
arp playback; Stop releases generated notes through their normal envelopes and
does not automatically retrigger physically held keys. Release/repress to play
manually. With latch on, release keeps the chord; the next new chord after all keys
are up replaces it. Turning latch off retains only physically held notes. Space
clears all voices and the latched chord immediately, keeping transport/latch settings.
Changing screens does not stop the clock. The screen shows actual audio step/note
status and eight moving step lamps; rate, gate and range edits run in audio.

Both native instruments compile the same fixed-size, allocation-free `Firmware/arp.h`
and `instrument.c` as the WASM modules. Playback is sample-clocked; no UI timers
schedule notes. Theme flash writes now also wait for transport to stop. Actual ADV
CPU timing and physical key behavior remain unmeasured.

## Tempo-synced arpeggiator (0.10.1)

Open Global (V), select Tempo, then OK. Arrows change BPM by 1; Shift+arrows by
0.1. Range 40–300 BPM, default 120. Edits apply live; OK or Esc returns to Global.
The Arp rate knob now selects 1/1, 1/2, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32,
or 1/32T; T means triplet. Default is 1/8 at 120 BPM (4 steps/sec). Tempo changes
preserve fractional position within the current step, including gate timing.
Tempo survives instrument changes but is RAM-only on ADV; AU state saves it on Mac/iOS.
Older arp modules remain free-running and retain the Hz control in the app.
This is the instrument's internal tempo; external host/MIDI clock sync is not implemented.

## LFO shapes and tempo sync (0.10.1)

N opens the LFO page: 1 Shape, 2 Rate, 3 Depth, 4 Sync; slots 5–8 unused.
Shapes are sine, triangle, square, saw and sample-and-hold. The graphic replaces
output scope on this page: grey full-scale shape, accent depth-scaled trace, and
a cursor/dot from the actual audio-phase/value snapshot. Sample-and-hold uses a
scrolling value history. Phase continues through silence and is not simulated in UI.

Sync defaults on, using Global BPM and musical divisions (including triplets).
Default half-note cycle at 120 BPM equals 1 Hz. Free mode restores the 0.05–20 Hz
control. Home knobs 7/8 remain rate/depth; rate follows the same Sync/Free setting.
Shape, rate, mode and tempo edits preserve LFO phase and filter memory. Depth
still defaults to zero; modulation destination remains cutoff. There is one shared
LFO across the four voices. Hardware timing and animation still need physical testing.
