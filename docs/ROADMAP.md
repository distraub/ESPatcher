# ESPatcher development platform roadmap

## Implemented

- Mac/iOS standalone and AUv3 targets with portable `.gsm5` WASM modules.
- Validated ABI 1 modules, isolated 256 KiB runtimes and execution fuel limits.
- AMSynth Analog and FM reference engines with native/WASM audio parity tests.
- Four voices, ladder filter, envelopes, tempo-synced LFO and arpeggiator.
- Shared Home/Global/OSC/LFO/Arp screen model, scope and LFO visualization.
- Eight encoder slots with page assignments and keyboard-based editing.
- Apple editor pad grid and selectable chord mode.
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
