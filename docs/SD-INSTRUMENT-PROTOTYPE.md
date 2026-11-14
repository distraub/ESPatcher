# SD instrument library prototype 0.1

ESPatcher remains installed as the launcher/runtime. `.espkg` files contain
engines, defaults and declarative UI scenes. The experimental ADV runtime reads
them from SD into RAM; selecting an instrument or engine does not use OTA or
rewrite an application flash partition. A computer is needed once to install
this runtime, and again for runtime updates—not for each package switch.

**Cross-compiled and tested on desktop, not tried on a physical ADV.** Dynamic
AMSynth CPU/RAM capacity is not established. This is a prototype format, not a
stable third-party SDK or a promise that any ESP32 firmware binary can run.

## Try the workflow on your computer

Serve the repo with `python3 -m http.server 8765 --bind 127.0.0.1` and open
http://127.0.0.1:8765. Choose **Library** (or L), then an instrument:

- **Signal Check:** tiny Tone/Pulse engines for the first on-device sanity check.
- **AMSynth:** Analog and FM engines with Play, OSC, LFO and Arp screens.
- **Orbit:** reuses the FM engine with new defaults, parameter-level display,
  Drift LFO page, custom knob labels/bindings and a scale pad layout.

The header selects engines *inside* the loaded instrument. Package scene buttons
and C/V/B/N select its pages. Global and Library are retained by ESPatcher.
Loading another package rebuilds the scene navigation, display layout, knob
assignments and pad pitches. Loading stops transport; it is not seamless audio
crossfading. Held notes are released.

You can open a local `.espkg` through Library. The browser verifies its module
hashes and only executes hashes listed in the server's `Library/catalog.json`.
Unknown DSP is refused because browser-native WebAssembly lacks our wasm3 fuel
limit. To preview your own DSP locally, build it and regenerate your *own trusted*
catalog. Package UI text is data, never HTML or script. Checksum matching detects
corruption; it is not a digital signature or proof of authorship.

The Apple standalone/AUv3 editor still imports `.gsm5`, not `.espkg`; package
import/UI adaptation there is a future pass. Its existing module memory contract
and session restoration are unchanged.

## Build the SD files

Prebuilt packages and reference modules are committed. To regenerate them:

```sh
# Only needed after changing DSP; requires a WASI SDK.
WASI_SDK_PATH=/path/to/wasi-sdk python3 Tools/build_instruments.py --rebuild-modules
# JSON/UI/default changes do not require the SDK:
python3 Tools/build_instruments.py
python3 Tools/prepare_sd.py
```

The last command stages `build/sd-card/ESPatcher/Instruments/*.espkg`, checksums
and the MIT license. Copy the **ESPatcher** folder to the SD root. The tool does
not format, mount or flash anything. No sidecar engine files are needed: every
package carries its WASM bytes. SD must be readable by the Arduino SD library;
use a suitable FAT-formatted card for the first test.

Authoring files are in `Library/Definitions/*.json`. Module paths are relative to
the definition file. `Firmware/Examples/hello.c` is the small sanity instrument;
Tone has level/attack/release, Pulse adds duty (0/1/2 = 25/50/75%). Its other
prototype parameter slots are deliberate no-ops. AMSynth is the full reference.

## Experimental ADV target

This target is separate from the existing native AMSynth hardware candidate:

```sh
pio run -d Device/ADV/SDPrototype
# Once the ADV arrives, install the runtime intentionally:
pio run -d Device/ADV/SDPrototype -t upload --upload-port YOUR_PORT
pio device monitor -d Device/ADV/SDPrototype --port YOUR_PORT
```

No upload has been performed by this development pass. The target uses the
existing ADV partition layout; G0/USB download recovery remains available.
The initial flashing procedure and board precautions are in the parent device
guide. Do not use this image on the original Cardputer.

The first ADV scanner lists up to eight valid packages; larger libraries and
search/paging are future work.

On-device controls:

| Action | Control |
|---|---|
| Return to resident Library | L or the G0/BtnA button |
| Select package / engine | Up/down, Enter |
| Rescan SD | R while in Library |
| Resume current instrument / leave engine list | Escape |
| Instrument pages | C/V/B/N, as shown by the package |
| Edit a virtual knob | Select 1–8, then arrows; Shift is fine |
| Notes | A W S E D F T G Y H U J K, mapped through the scene's pads |
| Octave / panic | Z/X / Space |
| Arp transport where supported | Minus |

Audio runs on core 1; SD, validation, compilation and UI run on core 0. A switch
requests audio suspension and waits for acknowledgement before freeing the old VM.
Only one engine VM is retained to save RAM. Compilation/allocation stay off the
audio task. SD is not read during rendering. Once loaded, playback does not need
the package file open.

Metadata/hash failure before suspension preserves the old engine. After releasing
the old VM, an allocation/compile failure leaves the resident Library running in
silence; it cannot keep two VMs for rollback. Module traps halt that engine to
silence, with Library still available. Pause timeout aborts the switch without
freeing the live VM. Device VM execution has fuel limits, but compilation/loading
latency and recovery behavior still need hardware testing.

The sample packages use 64 KiB guest memory and a 16 KiB interpreter stack. The
ADV experiment accepts 64/128 KiB guests only, after a heap/headroom check. Guest
memory is only part of the cost: module bytes, translated code, interpreter
metadata, task stacks, display and codec buffers also consume RAM. Heap checks
are conservative admission checks, not a proof that all later allocations fit.

SD uses the documented ADV pins: CS 12, MOSI 14, clock 40, MISO 39. The runtime
uses a lower 12 MHz SPI clock for this initial experiment. See
[M5's ADV pin map](https://docs.m5stack.com/en/core/Cardputer-Adv).

## First-device proof checklist

1. Boot without SD; ensure Library stays usable and audio is silent.
2. Insert SD, R, select Signal Check → Pulse. Record heap and largest block.
3. Play one and four notes. Require DSP blocks below the 8,000 µs deadline and
   no rising late-block count. Record audio and control task stack headroom.
4. Switch Pulse ↔ Tone repeatedly, then try AMSynth Analog/FM and Orbit.
5. Try a truncated/checksum-bad package and remove SD during loading.
6. Test L recovery from a faulted engine and R after SD insertion/removal.
7. Only then decide whether interpreted AMSynth meets the device's audio budget.

No physical boot, audio latency, deadline, SD-removal or fault-recovery claim is
made until these checks run on a real ADV. The small sanity engines distinguish
"runtime works" from "the full reference engine fits and is fast enough".

## Prototype package contract

Little-endian integers and IEEE float32; no compressed ZIP or external paths.
Magic `ESPKG001` (8 bytes), total length u32, package ID/name/version as length-u8
ASCII strings (max 48/48/16), then engine count u8 (1–4). Each engine contains:

1. ID/name strings (48/48), guest memory u32 (64/128/256 KiB in format).
2. Nineteen float32 defaults using the current synth profile; transport defaults
   to stopped. This is not yet arbitrary engine-owned parameter metadata.
3. Scene count u8 (1–4), followed by each scene: title/subtitle strings (24/48),
   layout u8 (scope/LFO/values/levels = 0/1/2/3), eight binding bytes (0–18, 255
   unused), eight label strings (max 8), sixteen pad-offset bytes (0–48).
4. Module length u32 (max 1 MiB), SHA-256 bytes (32), embedded `.gsm5` bytes.

IDs use lowercase a–z, digits, underscore or hyphen. Text is nonempty printable
ASCII for now. Packages are at most 4 MiB, with no trailing data. Duplicate engine
IDs, invalid layouts/bindings/defaults and unsupported memory sizes are rejected.
C++ and browser decoders share fixtures and rejection tests. Checksums are checked
separately before module publication. The resident library is not an instrument
scene and cannot be removed by its UI data.

These are bounded UI primitives, not arbitrary drawing code, new drivers or a
complete operating-system replacement. Wider UI/input schemas, engine capability
metadata, preset/state migration, package persistence in AU sessions, package
signatures and native Apple package UI remain future work.
