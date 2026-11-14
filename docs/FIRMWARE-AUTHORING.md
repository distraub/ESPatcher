# Designing an ESPatcher instrument

This guide separates the implemented module contract from the proposed authoring
workflow. ESPatcher is the loader/platform. AMSynth is our firmware family;
AMSynth Analog and AMSynth FM are currently separate engine modules.

## Terms and the user-facing model

| Term | Meaning |
|---|---|
| Engine | Executable DSP and musical behavior, with a stable engine ID |
| Instrument package | Proposed distributable containing an engine, descriptions and factory presets |
| Preset | Proposed named parameter/settings snapshot for one compatible engine; no executable code |
| Session | Host state, including loaded module bytes, parameter values and loader preferences as supported |
| Device firmware | Native ESP32 application image; distinct from the portable instrument module |

The proposed browser has separate **Engine** and **Preset** selections. Loading an
engine replaces code. Recalling a preset changes settings inside that engine.
A package update is not a preset change. Engine families can contain multiple
engines; AMSynth does not need to become the loader name again.

Today the selector is labeled Instrument and loads `.gsm5` engine modules.
There is no preset file format/browser or instrument-package importer yet.
AU fullState embeds the module bytes and parameters, which is session restoration,
not an implemented user preset library. The ADV image compiles the two engines
into one native application; it does not currently load WASM from microSD.

## Ownership

| ESPatcher supplies | Engine supplies |
|---|---|
| Module validation, lifecycle and isolation | Audio generation and signal processing |
| Themes, navigation and device keyboard conventions | Parameter behavior, smoothing, voice allocation |
| Knob/virtual-knob input routing and host parameter bridge | Sample-clocked modulation, arp and sequencing when supported |
| Global BPM control and transport commands | Interpretation of timing/transport and published phase/status |
| Shared screen renderers and output scope | Control descriptions and capabilities in the future schema |
| Host/session serialization | Well-defined initialization, note release and panic |
| Proposed preset browser/storage | Proposed factory presets and state compatibility information |

Themes and keyboard bindings should not be reimplemented by each instrument.
Custom engines should request reusable display types (scope, LFO, step grid, list),
not arbitrary native UI code. Only the output scope is universally inferred from
audio; modulation/sequence visuals must read truthful engine telemetry.

## What authors can build today

Read [GSM5-FORMAT.md](GSM5-FORMAT.md) for the normative exports and limits.
Current modules must choose an existing parameter profile. Profiles are contracts,
not merely parameter counts; do not repurpose cutoff as a different effect because
the address happens to be available.

| Profile | Parameters | Meaning |
|---|---:|---|
| mono-v1 | 5 | Shape, attack, release, cutoff, level |
| synth-v2 | 6 | Adds ladder resonance |
| synth-v3 | 8 | Adds free LFO rate and depth |
| synth-v4 | 14 | Adds free-rate arp controls/status |
| synth-v5 | 16 | Adds Global BPM and synced arp division |
| synth-v6 | 19 | Adds LFO sync, division, shapes and phase/value status |

The host infers available controls from this profile. Higher profiles must implement
the advertised behavior and required exports, rather than expose inert controls.
The current page/slot mappings are fixed in Core/Navigation.hpp, with legacy/free-rate
resolution in the adapters. Optional `knob_labels` changes Home captions only; it
does not define parameters, remap addresses, introduce pages or disable features.

Current `.gsm5` modules are WASM v1 with fixed 256 KiB memory and no imports,
filesystem, device drivers, network or wall-clock APIs. This is portable DSP
execution, not ESP32 instruction/peripheral emulation or arbitrary `.bin` loading.
Keep hardware access in the native adapter. The loader currently exposes notes,
scalar parameters, mono audio, and specified telemetry, not arbitrary resources.

## Engine implementation rules

1. Give the engine a unique, stable ID matching `gs_id()` and the manifest. Change
   its display name freely, but preserve IDs when updating a compatible engine.
2. Implement initialization for the actual sample rate. The host warms a note/render
   path and initializes again before publication. Avoid externally visible side effects.
3. Accept varying render lengths and preserve oscillator, envelope, filter, clock
   and modulation state across calls. Never derive timing from UI updates or block count.
4. Use fixed-size data and bounded work. No allocation, locks, logging, file I/O,
   JSON parsing, Swift/Obj-C calls or structural graph changes on the render path.
5. Clamp/reject invalid parameters, produce finite audio, honor note-off, and make
   panic immediately clear sounding voices and latched musical input.
6. Smooth continuous audible changes without clearing filter or modulation memory.
   A displayed phase must come from DSP; the UI may sample history for drawing.
7. Define Stop separately from panic. AMSynth Stop releases arp notes through
   their tails; panic clears audio and latched chords but retains transport settings.
8. Respect the eight physical slots and keyboard-only editing. Unassigned slots
   must be inactive. Keep useful primary controls on Home; show current assignments
   on every bank, with names, units and values.
9. Treat native and WASM implementations as the same engine. Interpreter speed is
   not a hardware benchmark; measure the physical ADV separately.

## Proposed generic authoring schema — not implemented

The next protocol revision should let engines describe these things explicitly:

- Stable parameter IDs/addresses, names, types, ranges, units, defaults, scaling
  and formatting; never silently reuse an existing ID for another purpose.
- Supported pages and up to eight slot bindings per page. Home and Global remain
  platform conventions; OSC/LFO/Arp/Sequence/Effects are optional capabilities.
- Display widgets and their telemetry contracts. Each needs a bounded, allocation-free
  snapshot interface, with timing/phase owned by audio.
- State schema version and explicit migrations. Engine version, ABI version and
  preset/state schema version are separate compatibility concerns.
- Factory presets, authorship/license information, and resource requirements.

Keep the AU parameter tree stable; do not rebuild it when an editor opens or a page
changes. A generic parameter schema needs a deliberate host-address mapping and
migration design before arbitrary engines can share it. Do not publish an example
manifest claiming custom pages work with today's loader.

## Proposed preset rules — not implemented

A preset should identify its engine and state schema, then store stable parameter
values and supported musical settings. It should not contain executable code,
loader themes/key maps, held notes, random runtime history or an instruction to
start playback. Global tempo belongs to loader/session state by default; a future
song/project format may explicitly include it.

On recall, validate compatibility before changing anything. An incompatible preset
must leave the current sound intact. Ordinary parameter recall should preserve
appropriate release tails, filter state and modulation phase. Engine replacement
uses the loader's separate swap lifecycle. Changed values should mark a preset
as edited; no preset should silently rewrite itself.

These rules describe the intended user preset system, not a change to existing AU
fullState: session state currently includes all host parameters and embedded code,
including transport parameters. Do not mistake that host state for a preset format.

## Development and acceptance

Start from Firmware/firmware.h, Firmware/instrument.c and Tools/build_firmware.py;
the current builder is an example-specific script, not a general packaging SDK.
Adapt its module ID/metadata and selected profile without changing stable IDs of
other engines. Include a license and reproducible source/build instructions.

Before sharing an engine, verify native/WASM agreement, finite/bounded output,
note-off/panic, voice stealing if applicable, block-size independence, sample-rate
changes, live parameter edits, multiple isolated instances, malformed-load rollback,
state restore and every advertised page/telemetry stream. Exercise keyboard-only
operation and inactive slots. Run the repository tests and relevant app/AUv3 builds.
Hardware releases also need measured DSP deadlines, heap headroom and physical
input/display checks. Desktop success alone is not hardware readiness.

## Recommended next platform milestone

Define and implement engine descriptions and preset identity/migration together,
then prove them with the two AMSynth engines before publishing a third-party
starter template. This avoids making today's nineteen-parameter synth profile the
permanent definition of what an ESPatcher instrument can be.

## Reference instrument and compatibility

AMSynth is ESPatcher's default/tutorial firmware family. Analog and FM currently
share the C reference implementation. GlitchSynth is reserved for a future
experimental derivative with its own engine ID; it has not been forked yet.

AMSynth modules 0.7.2 retain `pocket_analog` / `fm_bell` IDs, synth-v6, ABI 1 and
all parameter addresses. Apple bundle identifiers, AU component codes and stored
preference keys also remain unchanged. Older AU sessions embed their old module
bytes, so they may still display the former GlitchSynth name until a newer module
is loaded; preserving session audio takes precedence over rewriting its metadata.

Start with [GETTING-STARTED.md](GETTING-STARTED.md). The current reference includes
more features than a minimal tutorial; an independently buildable small starter
template is a planned next step, not an existing SDK feature.
