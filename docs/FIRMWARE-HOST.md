# Firmware host direction

Historical design notes. The first loader is now implemented; see
[GSM5-FORMAT.md](GSM5-FORMAT.md) for the current contract, tests and limits.

## Intent

A RemoteFM1-style instrument lab: load an instrument, interact with a virtual
Cardputer, hear its DSP, then build that instrument for the physical ADV.
The standalone Mac/iOS host and AUv3 should use the same instrument runtime.
Arbitrary existing ESP32 firmware binaries are outside this contract.

## Current implementation

- Independent original C++ mono voice in Core/Synth.hpp, no platform dependencies.
- Native Mac and iOS apps, AUv3 targets, shared SwiftUI editor and Objective-C++ bridge.
- Flat AU-owned parameters built at AU initialization, before any editor exists.
- Fixed-capacity main-thread-to-render note queue, atomics for scalar parameters.
- Sample-offset MIDI 1.0 and parameter events; preset state through fullState.
- PolyBLEP saw/square, sine, attack/release, one-pole low-pass and output smoothing.

There is no firmware importer, VM, hardware firmware, or module switcher yet.
AU ramp events currently adopt their target at event time; exact ramp interpolation,
MIDI 2.0, external MIDI input in the standalone host, audio-route restart handling,
and persisted standalone sessions are not implemented. The on-screen keys are a
chromatic strip, not a replica of all 56 physical keys. This core's filter differs
from the browser prototype's biquad. The original browser remains a separate study.

## Shared module contract (draft)

Keep the instrument independent from M5Cardputer, AVFoundation, SwiftUI and browser
APIs. A versioned C ABI should expose:

1. Descriptor: ABI version, instrument ID, version, memory budget and parameters.
2. Preparation: actual sample rate, maximum block frames and fixed memory region.
3. Events: bounded note/key/encoder/parameter events with sample offsets.
4. Render: float mono/stereo blocks, no allocation, file access or UI calls.
5. Display: RGB565 240 × 135 framebuffer updated on a control tick, never audio.
6. State: versioned bounded bytes; host does storage on a non-audio thread.

Use sample counters for musical time. Display timing must never drive the notes.
External controls are abstract events so knobs and larger screens can be added.
The AU's stable parameter slots need an explicit module mapping; changing modules
must not recreate the parameter tree. Instances must retain independent state.
Do not reuse GlitchDrums' large render core: its sample pools are not an ADV budget.

## Two executable formats, one source

Initial practical path: compile module source natively for ESP32 and for Apple.
That gives the hardware native-speed DSP and allows computer/offline parity tests.
It does not provide runtime loading of custom source on iOS.

For importable firmware packages, evaluate a WebAssembly interpreter in the Apple
host and a native build of the same module source on ADV. A package could contain
the WASM preview, an explicitly compatible hardware image, manifest, and source
identity. Compare outputs from both targets with deterministic fixtures. Matching
source alone does not guarantee bit-identical floats or hardware performance.
An ESP32 image is flashed, never executed by the Apple host.

Running the exact same WASM file on ADV is a separate experiment. wasm3 supports
ESP32, but documents Xtensa tail-call/stack-performance concerns:
https://github.com/wasm3/wasm3/blob/main/docs/Hardware.md
Do not promise real-time capacity before measuring audio deadline misses, stack,
RAM, and UI contention on the shipped unit. An interpreter must not JIT on iOS.

## Import and switching requirements

Validate ABI, bounds, exports, memory limits, checksums and capabilities off the
audio thread. A checksum identifies bytes; it does not establish trust. Instantiate
and warm the candidate off-thread. Quiesce or exchange at a block boundary, fade
out/in, and retain the prior instance until the audio thread has released it.
Loading must be per AU instance and saved state must identify its module/version.
Persist a recoverable known-good instrument; failed loads must leave it playable.
An unbounded interpreter call cannot safely live in the audio callback: a runtime
must have enforceable execution budgets or use a bounded worker/ring-buffer design.
Keep file/network/platform APIs out of the guest imports.

App Store distribution of user-imported executable modules needs a separate review
of Apple's current rules; an interpreter is not an automatic exemption:
https://developer.apple.com/app-store/review/guidelines/ (2.5.2 and 4.7).

## Next proving steps

1. Native Mac standalone launch and Apple AU validator pass; third-party DAW editor testing remains.
2. Implement the C module ABI with two tiny example instruments and offline tests.
3. Build the simple voice for ADV, measure memory/CPU and verify output/keys.
4. Add a WASM preview loader to the Mac host; test traps and deadline overrun paths.
5. Integrate validated packages and per-instance state into AUv3/iOS.
6. Only then decide whether identical-binary execution on ADV is viable.

Reference reviewed locally: GlitchDrums project.yml, AudioUnitHost.swift,
GlitchDrumsAudioUnit.mm, AudioUnitViewController.swift, DSPKernel and RemoteFM1's
adapter. This implementation follows their layering without copying their engine.
