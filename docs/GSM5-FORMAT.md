# ESPatcher .gsm5 instrument module format, version 1

This is a Cardputer-oriented portable instrument format inspired by RemoteFM1's
WASM files and manifests. It is independently implemented and is not compatible
with `.rfm1` or arbitrary ESP32 firmware images. It currently carries audio DSP,
not a framebuffer or a full emulated device.

See [FIRMWARE-AUTHORING.md](FIRMWARE-AUTHORING.md) for engine/preset terminology and
the distinction between current profiles and the proposed generic authoring schema.

## File and manifest

A WASM v1 module, at most 1 MiB, containing exactly one custom section named
`gs5`. Its UTF-8 JSON payload has these required fields:

```json
{
  "format": 1,
  "abi": 1,
  "id": "pocket_analog",
  "name": "AMSynth Analog",
  "version": "0.7.2",
  "polyphony": 4,
  "author": "Alex Matheu",
  "memory_bytes": 262144,
  "target": "cardputer-adv",
  "parameter_profile": "synth-v6"
}
```

ID is 1–31 lowercase ASCII letters, digits or underscores. Name is 1–64
characters, version 1–32. Author is descriptive. Format and ABI must equal 1.
Runtime `gs_id()` must match the manifest ID. The file's bytes are embedded in
AU fullState, so restoring does not depend on a path outside the AU sandbox.
No global firmware library or App Group is needed for this first version.

Modules have one fixed memory: initial=max=4 pages (256 KiB). No imported
functions, memories, tables or globals; no WASI, files, network, clocks or JIT.
WASM start sections are refused. `_initialize` is optional, `() -> ()`.
The loader caps type/function counts at 512; globals/exports/element segments
at 64; data segments at 128; one function table with at most 64 entries.
Unsupported WASM features are rejected by wasm3. This is a local development
runtime; parser fuzzing and a hostile-input security audit are still outstanding.

## Required exports

All integers and pointers are wasm32 i32. Memory output is little-endian float32.

| Export | Signature | Meaning |
|---|---|---|
| gs_version | () → i32 | ABI, must be 1 |
| gs_id | () → i32 | NUL-terminated ID string, at most 63 bytes |
| gs_init | (i32 sampleRate) → () | Reset at actual host rate, 8–192 kHz |
| gs_note | (i32 note, i32 velocity) → () | MIDI note; velocity 0 releases |
| gs_parameter | (i32 address, f32 value) → () | Stable parameter slots below |
| gs_panic | () → () | Clear gates/envelopes immediately |
| gs_voice_limit | () → i32 | Optional, maximum voices 1–8 (older modules default to 1) |
| gs_active_voices | () → i32 | Optional, sounding voices including release tails |
| gs_render | (i32 frames) → i32 | Pointer to contiguous mono float samples |

Modules support 1–4096 frames. The interpreter host currently invokes render in
chunks of at most 128. Samples must be finite. Host output duplicates mono into
stereo and clamps to ±0.9. Buffer bounds/alignment are checked on every call.

## Parameter profiles mono-v1 and synth-v2

| AU address / stable ID | Range | Pocket Analog | FM Bell |
|---|---|---|---|
| 0 / wave | 0–2, indexed | Sine / Saw / Square | FM ratios 1 / 2 / 3 |
| 1 / attack | .005–2 seconds | Attack | Attack |
| 2 / release | .01–3 seconds | Release | Release and natural decay |
| 3 / cutoff | 80–16000 Hz | Four-pole ladder cutoff | Four-pole ladder cutoff |
| 4 / volume | 0–.5 linear | Output gain | Output gain |
| 5 / resonance (synth-v2 only) | 0–1 | Ladder resonance | Ladder resonance |

`mono-v1` specifies five slots; `synth-v2` appends resonance at address 5. The
host sends only five parameters to legacy modules. The profile is not a
voice-count restriction. Current example modules are four-voice instruments.
The AU tree and addresses stay stable across loads. Modules cannot invent new
AU parameters in this ABI. The current nineteen-parameter host tree is fixed at AU init;
legacy records restore missing values with the documented defaults below. If present, manifest `polyphony` must match `gs_voice_limit`. Old modules without
voice diagnostics retain compatibility; the editor omits the unavailable live count.
Musical timing and notes belong to the render code.
AU ramp events still adopt the target at the event offset; exact ramp interpolation
and MIDI 2.0 are not implemented.

## Loading, ownership and failure

The editor reads and validates a file, constructs a fresh isolated runtime,
compiles all functions, checks signatures/identity and warms a note/render path
on a background task. Failed loads preserve the previous instrument and parameters.
Two slots separate preparation from audio ownership. An acquire/release pending
index publishes at a render-block boundary; the previous slot is only reclaimed
by the control thread after the audio thread acknowledges the switch. No VM,
vector, NSData, JSON or file operation is created or destroyed inside rendering.
A 128-sample transition ramps from the prior output sample into the new instrument;
notes held before the switch are dropped. Notes queued after publication survive.

Each AU owns its runtimes, queue and state. Concurrent requests are serialized;
an unconsumed pending request is refused, rather than overwritten. Parameter
values are retained across switches. `gs_init` resets the module on sample-rate
reallocation. Built-ins are actual bundled `.gsm5` files loaded through the same API.

Initialization/metadata use 1,000,000 dispatches per call; parameter/note/panic use
100,000; each ≤128-frame render uses 1,500,000. Fuel, WASM traps, bad pointers and
nonfinite samples halt the instance, which renders silence until reloaded. These
budgets prevent an endless guest loop; they do not guarantee a real-time wall-clock
deadline. Long bounded calls can still cause dropouts. Device benchmarks and more
restrictive admission budgets or a worker-buffer host remain future work.

AU fullState stores parameters and complete module bytes. It intentionally does
not restore held notes or DSP tails. Legacy states without module bytes currently
restore parameters into the active instrument. No automatic standalone session
save or firmware upload/flashing is implemented.

## Build and Cardputer path

Run `WASI_SDK_PATH=/path/to/wasi-sdk python3 Tools/build_firmware.py`.
Checked-in modules let users run the app without installing a WASI toolchain.
`--fixtures` also builds the intentionally broken test modules outside app resources.
`sh Tools/test.sh` checks the native core, interpreter, native/WASM parity, and AU.

`Firmware/firmware.h` is the portable C API. `Firmware/instrument.c` builds both
examples; select `INSTRUMENT=0` (Analog) or `1` (Bell). The `Device/ADV` target now
compiles that source natively and calls the same functions for parameters, notes
and audio. It includes codec, keyboard, LCD and buffering adapters; physical
hardware validation is still pending.
Native vs WASM tests compare audio to within 0.0002 at 32/44.1/48 kHz; this is not
an on-device timing or CPU result. FM Bell is a simple proof-of-concept FM voice
and can alias at high notes/ratios.

Running the identical `.gsm5` in an ADV interpreter is not yet proven. A native
ESP32 build of the same source is the first hardware target, preserving DSP speed.
The browser study is unchanged and does not load these modules yet.

## References and attribution

- https://github.com/Quixotic7/RemoteFM1 — hardware panel/transport reference.
- https://github.com/Quixotic7/RemoteFM1IOS — firmware host design reference.
- https://github.com/wasm3/wasm3/tree/v0.5.0 — MIT interpreter, vendored with notices.

No RemoteFM1 GPL implementation files are included in this project.

## synth-v3 (module 0.4.0)

`synth-v3` retains addresses 0–5 from `synth-v2` and appends address 6 (`lfoRate`, 0.05–20 Hz, default 1 Hz) and address 7 (`lfoDepth`, 0–1, default 0). Both instruments use a shared free-running triangle to modulate cutoff up to ±3 octaves. ABI remains 1. The loader still accepts mono-v1 (5 parameters) and synth-v2 (6 parameters); LFO controls are disabled for those modules. Five- and six-parameter AU states restore with 1 Hz / zero depth. Visual knob order is independent of the stable parameter addresses.

## Instrument screen labels (module 0.4.1)

An optional `knob_labels` array supplies exactly eight display labels, in physical knob order: `["CUTOFF","RES","WAVE","ATTACK","RELEASE","LEVEL","RATE","DEPTH"]`. Each label is 1–7 characters from uppercase ASCII letters, digits, spaces, slash, and hyphen. Invalid labels reject the load before publication, preserving the current instrument. FM Bell uses RATIO for slot 3. Missing labels use the profile defaults, with SHAPE for slot 3. Legacy profiles display unsupported slots as UNUSED. Labels do not remap parameter addresses: current profiles bind slots to `[3,5,0,1,2,4,6,7]`. Fully arbitrary parameter layouts remain future work.

## synth-v4 (module 0.5.0)

Addresses 0–7 are unchanged. New parameters:

| Address | ID | Range | Default |
|---|---|---|---|
| 8 | arpRunning | 0/1 | 0 |
| 9 | arpPattern | 0 Up, 1 Down, 2 Up/Down, 3 Random | 0 |
| 10 | arpRate | 0.25–20 steps/sec | 4 |
| 11 | arpGate | 0.05–1 | 0.5 |
| 12 | arpOctaves | 1–4 | 1 |
| 13 | arpLatch | 0/1 | 0 |

The profile requires `gs_arp_status() -> i32`: bit 31 is running; bits 8–30 are the
step counter; bits 0–7 are generated MIDI note + 1 (0 means gate off). This is
polled from audio, then published atomically to the UI. ABI stays 1. The host still
loads five-, six- and eight-parameter profiles. Old AU parameter states restore
arp defaults with transport stopped. The eight manifest `knob_labels` still describe
Home; Arp binds physical slots to `[9,10,11,12,13,8,-1,-1]`.

## synth-v5 (module 0.6.0)

Appends address 14 `tempo` (40–300 BPM, default 120) and address 15 `arpDivision`
(index 0–8, default 3): 1/1, 1/2, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T.
Quarter-note beat durations are 4, 2, 1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12.
`synth-v5` calculates step Hz = BPM / (60 × beats); address 10 remains reserved
for legacy free-running rate compatibility. The Arp bank now binds slot 2 to 15.
The loader still accepts synth-v4 and sends only its first 14 parameters; its Arp
page retains a Hz knob. Missing tempo/division in old states default to 120 and 1/8.
ABI stays 1 and the arp status export is unchanged.

## synth-v6 (module 0.7.0)

Adds address 16 `lfoSync` (0 free / 1 synced, default 1), 17 `lfoDivision` (same
nine note divisions as arp, default 1 = half note), and 18 `lfoShape` (0 sine,
1 triangle, 2 square, 3 saw, 4 sample/hold; default 1). Address 6 remains free Hz.
The LFO page binds `[18,17,7,16,-1,-1,-1,-1]`; when free, rate resolves to address 6.
Home slot 7 similarly resolves to 17 when synced or 6 when free. Older profiles
keep their original rate/depth page and have no simulated phase animation.

`gs_lfo_status() -> i32` is required: upper 16 bits encode phase / 65536, lower
16 bits are signed raw value / 32767. Audio publishes this snapshot atomically;
UI draws the shape and phase without advancing an oscillator. Sine uses the shared
fast polynomial approximation (maximum tested error <0.0012). Sample/hold is seeded
per instance and updates only at phase wrap. ABI remains 1; old module profiles and
state arrays through 16 parameters remain supported. Missing new state values
restore synced half-note triangle defaults.
