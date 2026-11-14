# Firmware loader validation — November 13, 2026

- Mac standalone and AUv3 builds: pass.
- iOS app and AUv3 simulator compile: pass; not launched on a physical device.
- Mac app launch and Pocket Analog → FM Bell UI switch: observed.
- Apple auval aumu/gsm5/MSND, component version 2: full validation pass.
- Native core, WASM loader, module/native audio parity and AU integration tests: pass.
- Interpreter fixtures: infinite loop fuel, invalid output bounds and guest trap halt to silence.
- Failed import preserves instrument; immediate queued note after switching is audible in offline render.
- Twenty repeated module swaps and embedded module state restore before/after AU allocation: pass.

Local logs: build/auval.log and build/tests.log (ignored build artifacts).
Reproduce regression checks with `sh Tools/test.sh`.

Physical ADV, physical iOS, third-party DAW editor, hardware latency/CPU, and
hostile-input fuzz/security testing remain outstanding. Native/WASM audio parity
uses a 0.0002 tolerance at 44.1/48 kHz; it does not establish device capacity.


## ADV 0.2.0 hardware candidate

- ESP32-S3 build and merged-image packaging: pass.
- Target: 8 MB flash, 240 MHz, no PSRAM; pinned M5Stack drivers.
- Static RAM: 59,012 bytes / 327,680 bytes. Runtime task/driver heap is additional.
- Flash: approximately 544 KB / 3 MiB application slot.
- 32 kHz / 256-frame native audio producer and playback ownership callbacks.
- Control tests: equal-count key swaps, releases, octave/panic key suppression,
  instrument switch, key-repeat timing, parameter and octave limits pass.
- Native/WASM parity now includes 32 kHz in addition to 44.1/48 kHz.
- Merged binary components verified at offsets 0, 0x8000, 0xe000, 0x10000;
  partition layout, ESP32-S3 image checksums/hash and package SHA256 checked.
- No board connected or flashed. Boot, actual sample clock, sound quality,
  latency, headphone/speaker behavior, DSP deadlines and heap require hardware.

See Device/ADV/README.md for the first-device checklist and rollback path.


## Four-voice pass: modules 0.2.0 / ADV 0.3.0

- Four-voice spectral, allocation/stealing, release, panic and full-MIDI-range tests pass.
- Native/WASM chord/steal audio parity passes at 32/44.1/48 kHz.
- Legacy mono compatibility and ordered panic-then-chord AU behavior pass.
- Mac standalone/AUv3 and iOS app/AUv3 simulator builds pass.
- ADV compile/package passes: 66,988 bytes static RAM, 541,209 bytes flash.
- UI core 0 / audio core 1; repeating four-note device test on key 8.
- Physical performance is unmeasured; see POLYPHONY.md for the timing targets.

Mac UI observed 4/4 active voices on Test chord and 0/4 after release.


## Ladder / knobs / themes pass: modules 0.3.0 / ADV 0.4.0

- Four-pole TPT ladder derived from GlitchDrums, high-end self-oscillation tested.
- Resonance peak, slope, state retention and extreme cutoff stability tests pass.
- Native/WASM parity covers resonance 0/.8/1, live edits, chords and stealing.
- Address 5 appends resonance; legacy five-value state restores to resonance 0.
- Legacy modules load with resonance disabled and receive only five parameters.
- GlitchDrums StudioKnob face/indicator port, logarithmic drag and double-tap reset.
- 38 exact FM1 palettes generated for SwiftUI/RGB565 from Assets/themes.json.
- ADV compile: 69,220 bytes static RAM, approximately 544 KB flash; four voices retained.
- Mac/iOS app and AUv3 builds pass. Physical audio/CPU tests remain pending.

## Performance screen pass

Scope tests cover initial silence, rising-crossing alignment, decimation independent of block size, nonfinite input, bounds, simultaneous writes/snapshots, and silence after audio. AU tests verify actual rendered scope energy, loaded-instrument label changes, and rollback when a module has invalid labels. Existing DSP, LFO, polyphony, compatibility and state tests pass. Mac and iOS builds and ADV package are checked separately. Physical display timing and heap headroom remain unmeasured.

## Navigation pass

Shared state-machine tests verify Fn shortcut suppression until release, Home/OSC/LFO bank mappings, page selection memory, held and one-handed virtual knobs, repeat delay/acceleration/fine edits, preview cancellation and saving, instrument load confirmation and notes continuing through page changes. Existing DSP/scope/AU tests remain passing. Native Mac runtime checks cover one-handed keyboard editing, fine/repeat edits, Command page shortcuts, theme preview/cancel/confirm, and instrument confirmation. Actual simultaneous held-number+arrow input is covered in the shared tests; the UI automation API cannot hold two non-modifier keys together. Physical ADV keyboard behavior and NVS persistence require the device.

## Arpeggiator pass

Pattern/gate tests verify Up, Down, endpoint-free Up/Down, deterministic Random,
sorted octave expansion with MIDI bounds, first-step timing, fractional sample
intervals and gate lengths at 32/44.1/48 kHz, latch replacement/release, and rate
phase continuity. Native audio tests cover exact block-size independence, release
tails on stop, panic clearing the latched chord while retaining settings, and quick
latched input after panic. Native/WASM comparisons run with arp stopped and playing,
including a live stop. AU tests cover 14 parameters, generated status, state restore,
legacy eight-parameter state defaults, and capability detection for older modules.
Navigation tests verify M/Arp mappings and edge-triggered Play/Stop. Mac runtime checks
verify M, minus repeat suppression, four-note input, step animation, latch and Stop.
Actual ADV CPU timing and keyboard tests require the hardware.

## Tempo-sync pass

All nine musical divisions are checked at 32/44.1/48 kHz, including fractional
triplet intervals and tempo changes preserving phase. Native/WASM comparison now
includes live BPM and division changes while the arp runs. Navigation tests verify
Global Tempo coarse/fine edits; AU tests verify 16-parameter state, tempo/division
restore, 14-parameter legacy defaults and legacy arp capability detection. Mac runtime
checks cover Global Tempo, 1 BPM/0.1 BPM editing, musical rate readout, triplet playback,
and live BPM changes. ADV physical clock/keyboard timing remains unmeasured.

## LFO shape/sync pass

Tests cover all five waveforms, bounded values, sine approximation error <0.0012,
synced phase across all divisions at 32/44.1/48 kHz, live tempo continuity,
sample/hold update boundaries and deterministic state, and phase/value telemetry
quantization. Native/WASM parity runs all five shapes, arp on/off, and live tempo,
division and transport changes. AU tests check new state restoration, legacy
16-parameter defaults and older-module capability detection. Runtime checks cover
shape/depth feedback, moving phase cursor during silence, sample/hold history,
Sync/Free rate rebinding and page-specific knob numbering. Actual ADV performance
and physical display behavior remain unmeasured.

## ESPatcher / AMSynth platform pass — November 14, 2026

- AMSynth modules 0.7.2 retain the previous engine IDs, ABI and addresses.
- Full native/WASM/audio/AU/navigation/arp/LFO regression suite: pass.
- Mac and iOS app/AUv3 compile: pass.
- Native ADV 0.10.1 cross-build and merged-package verification: pass; not flashed.
- Browser AudioWorklet renders the actual bundled modules: block parity with
  direct WASM, four voices, isolated instances, panic and telemetry: pass.
- Browser audio startup, Analog/FM switch, chord selection and LFO page: observed.
- MIDI Learn tests: exact channel, reserved CC, first-message no jump, source/target
  reassignment, capacity, range scaling and clear: pass.
- AU tests inject scheduled and host render-event MIDI through the actual unit,
  then verify learned values, UI synchronization and mapping state restore: pass.
- Browser MIDI router: first-message capture, exact channels, reassignment,
  reserved controls and validated mapping restore: pass.

Physical MIDI routing/controllers and third-party AU host MIDI Learn remain to be
validated. Browser Web MIDI permission/device behavior varies by browser and has
not been verified with a physical controller here. Device MIDI input, runtime SD
package loading, arbitrary UI ABI and the sequencer are not implemented.

## SD package prototype — November 14, 2026

- C++/browser package decoders and authoring builder: pass, including truncated
  data, invalid layout/binding/default, identity/manifest and checksum rejection.
- Thirty sequential RAM-only engine constructions/renders across three packages
  and five engine entries: pass on desktop. Not device timing.
- Browser AudioWorklet parity/isolation/panic tests now include all four small
  package modules as well as the original two 256 KiB modules: pass.
- Browser Library, AMSynth Analog/FM engine switch, Orbit's custom scenes/pads,
  Signal Check Tone/Pulse and L recovery navigation: observed locally.
- Experimental ADV SD/WASM runtime cross-compiles. Static linker RAM/flash
  numbers exclude guest memory, translated code, task stacks and most live heap.
- Default Mac/iOS loader and native ADV candidate remain separate from this
  experiment. The Apple editor does not yet load `.espkg` packages.

No ADV SD mounting, actual package loading, live heap, render deadlines, repeated
switches or recovery behavior has been tested on physical hardware. Follow the
[prototype checklist](SD-INSTRUMENT-PROTOTYPE.md) before calling it device-ready.
