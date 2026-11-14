# ESPatcher development platform roadmap

## Implemented

- Mac/iOS standalone and AUv3 targets with portable `.gsm5` WASM modules.
- Cross-platform browser host using the same bundled modules in an AudioWorklet.
- Standalone CoreMIDI, AU host MIDI, browser Web MIDI and parameter CC Learn.
- Validated ABI 1 modules, isolated 256 KiB runtimes and execution fuel limits.
- AMSynth Analog and FM reference engines with native/WASM audio parity tests.
- Four voices, ladder filter, envelopes, tempo-synced LFO and arpeggiator.
- Shared Home/Global/OSC/LFO/Arp screen model, scope and LFO visualization.
- Eight encoder slots with page assignments and keyboard-based editing.
- Apple and browser editor pad grids and selectable chord mode.
- Native Cardputer ADV build and offline-verified flash packaging.

Hardware boot/audio/latency/CPU are still untested. The current module profiles
are fixed; this is an evolving platform, not a stable general-purpose SDK.

## Next: make authoring reproducible

1. Validate the reference build and controls on a physical Cardputer ADV.
2. Add a minimal independent instrument template and step-by-step walkthrough.
3. Define stable engine-owned parameter metadata, pages and capability negotiation.
4. Separate engine loading from preset browsing and version preset state schemas.
5. Route 8Encoder and NeoTrellis hardware through shared logical control events.
6. Bring chord mode to native ADV and develop the sample-clocked sequencer.
7. Add controller-free sequence editing and consistent pad-light feedback.
8. Establish portable CI, release artifacts and external author conformance checks.

## Instrument direction

AMSynth stays a focused, readable reference. GlitchSynth will be a separate
experimental instrument derived from that foundation, with a new engine ID.
No separate GlitchSynth module exists yet. Shared components should have clear
ownership and tests rather than forcing both instruments to evolve together.

See [FIRMWARE-AUTHORING.md](FIRMWARE-AUTHORING.md) for proposed versus implemented
contracts. Design proposals in that document are not accepted loader APIs yet.

## Persistent device runtime

The desired normal operation is loading SD instrument packages without rewriting
internal flash. Packages should be able to supply multiple engines and their own
UI/workflow. See [DEVICE-RUNTIME.md](DEVICE-RUNTIME.md). This depends on device
runtime memory, execution and UI scheduling benchmarks; it is not implemented.
An SD-to-OTA installer is a distinct fallback and still flashes internally.

## SD prototype milestone

Implemented experimentally: bounded `.espkg` packages, multiple engines, default
parameter sets, package-defined screen primitives/knob assignments/pad pitches,
browser Library, package builder/SD staging, and a separate ADV SD/WASM launcher
that cross-compiles. See [the prototype guide](SD-INSTRUMENT-PROTOTYPE.md).
Physical heap/deadline/recovery testing, native Apple package UI and arbitrary
parameter/UI contracts remain outstanding.
