# Proposed on-device instrument library

**A first experimental implementation now exists:** see
[SD-INSTRUMENT-PROTOTYPE.md](SD-INSTRUMENT-PROTOTYPE.md). It provides a browser
package workflow and a separate ADV SD/WASM runtime that cross-compiles but has
not been tested on hardware. The broader UI/API goals below remain a roadmap;
only the bounded prototype scene contract is implemented.

ESPatcher should be the persistent Cardputer ADV runtime and launcher. Its role
is above the ESP32 bootloader: initialize hardware, mount SD, present the library,
load a package, and remain available to leave or recover an instrument. The ADV
is an ESP32-S3 microcontroller; our native build uses Arduino/FreeRTOS, not Linux.

## Intended experience

Copy an instrument package to SD → Library → choose instrument → choose engine.
A package can provide multiple sound engines and replace the instrument UI,
page navigation, note layout, pad roles and musical workflow. A new drum machine
need not inherit AMSynth's synth pages or eight fixed labels. A package may also
be a sequencer or generative instrument using the same device services.

Normal instrument changes should load from SD into the runtime without writing
application flash. ESPatcher itself still needs firmware updates when the
runtime, drivers or platform contract changes.

## Stable runtime responsibilities

- Audio codec, sample clock and bounded input/event delivery.
- Display, keyboard, MIDI, SD, optional encoder/pad hardware services.
- Library and package validation, ABI/capability negotiation and lifecycle.
- Reserved return-to-library/recovery gesture independent of instrument pages.
- Fault reporting, note release and silence before reclaiming an engine.
- Storage namespaces for each instrument's presets, settings and sequences.

Instruments own their music and normal user experience. The loader's recovery
surface remains available even when an instrument replaces every ordinary page.

## Package and UI contract to design

The current `.gsm5` ABI exports DSP and telemetry, not arbitrary screens or input
handlers. The prototype layers declarative scenes around this DSP ABI. Current Analog and FM are separate `.gsm5` modules; the ADV application
compiles both engines in. They are not yet a dynamically loaded multi-engine
package. The existing Apple/browser page model is an AMSynth reference interface,
not the final universal interface.

A stable future package needs a manifest declaring its identity, version, engine list,
ABI requirements, optional controller capabilities and state-schema versions.
The package format, extension, signatures and migration policy are not settled.
Do not author against guessed fields or undocumented UI exports.

For UI, evaluate engine-owned lightweight draw commands or scene descriptions,
plus logical input handlers. The host should render controls, scopes, labels and
step grids without letting UI work block the audio deadline. Full-frame buffers
are another option, but cost memory and transfer bandwidth. A separate UI runtime
also costs memory. Benchmark before selecting the contract; merely exporting a
new drawing function on the audio interpreter is not a timing solution.

## Hardware proof gates

1. Measure the native reference's static RAM, task stacks and live heap on ADV.
2. Prototype one dynamically loaded engine with bounded execution and memory.
3. Measure four-voice render worst case, control/UI work and SD-load latency.
4. Stop audio during loading; validate and initialize away from the audio task.
5. Test repeated switches, bad/missing packages, SD removal and return-to-library.
6. Only then establish device module/UI memory and CPU budgets.

The host preview's 256 KiB module budget is not a proven ADV runtime budget.
A large SD card provides storage, not extra execution RAM or processor capacity.
A device interpreter or alternative relocatable native-module strategy must be
proven; desktop/browser success alone does not establish hardware feasibility.

## Whole-firmware fallback

The current partition table already reserves `ota_0`, `ota_1` and `otadata`.
An SD installer could copy a compatible app image to the inactive slot, verify
it, then reboot, without a computer. That still writes internal flash and needs
a compatible recovery path. It is an alternative to portable instrument loading,
not the promised flash-free library workflow, and is not implemented yet.

Sources: [Cardputer ADV hardware](https://docs.m5stack.com/en/core/Cardputer-Adv),
[ESP32-S3 OTA model](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html).
