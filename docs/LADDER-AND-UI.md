# Ladder resonance and Glitch-family controls

ADV candidate 0.4.0; instrument modules 0.3.0; AU component build 4.

## Reference review

- GlitchDrums `GlitchDrumsExtension/DSP/GDLadderFilter.hpp`: selected four-pole
  TPT ladder, feedback solve, four saturated stages, max feedback 4.2, fastTan.
- GlitchBreaks2 `DSP/Effects/processors/GBResonantFilterEffect.hpp`: reviewed the
  two-pole state-variable filter and its resonance/drive/compensation treatment.
- GlitchDrums `StudioKnobFace.swift`, `StudioKnobIndicator.swift` and
  `VoiceParameterKnob.swift`: source of knob appearance and gesture behavior.
- GlitchDrums FM1 `src/themes.h`: all 38 RGB565 palettes, role order and names.

The original GlitchDrums ladder is included under Tests/Reference for response
comparisons; the production implementation is portable C in Firmware/ladder.h.
No source in the reference projects was modified.

## DSP adaptation

Four per-voice TPT integrators replace the one-pole filter. The linearized
feedback solve and stage topology follow GDLadderFilter. Feedback 0–4.2 reaches
seeded self-oscillation at the high end; note envelopes still close the output.
Panic clears the integrators. Continuous parameter edits never reset them.

The embedded saturation uses a 257-entry interpolation table for the bounded
rational tanh approximation x*(27+x²)/(27+9*x²), clamped at ±3. The maximum
absolute difference from tanh is below .025 in the test sweep. It is voiced
similarly, not bit-identical to the full app's per-stage tanh. The prewarp uses
GDLadderFilter's fastTan rational approximation. Coefficients are shared across
voices and smoothed at a 10 ms time constant; the feedback solve coefficient is
only recomputed while controls are moving. No trig or allocation occurs in the
per-voice sample loop. Existing output headroom/peak bounds remain in place.

The ADV cutoff control caps at .45 * 32 kHz = 14.4 kHz. The app parameter range
remains 80–16000 Hz, with the DSP applying the actual-rate cap. Static RAM is
about 69 KB; physical four-voice DSP timing remains unmeasured. Use key 8 and
diagnostics on the device when it arrives, including max resonance and sweeps.

## Parameter/state compatibility

Resonance appends AU address 5, ID `resonance`, range 0–1, default 0. Existing
addresses and IDs are untouched. The new `synth-v2` module profile accepts six
parameters; legacy `mono-v1` receives only the original five. Legacy five-value
AU states restore with resonance zero. Old modules disable the resonance knob.
The module bytes and six parameter values are embedded in current fullState.

## Knobs and palettes

The StudioKnob face and indicator were ported with theme bindings changed at
the edge. SynthKnob adapts the source drag lifecycle without the drums-only
step-lock UI: 180 pt vertical full-range travel, 270° indicator, logarithmic
cutoff/envelope travel, stepped shape, double-tap default, accessibility adjust,
and cleanup/commit at gesture end or disappearance. Preview writes directly to
the AU; only the editor mirror commits at the end of a drag. DSP owns audio state.

Assets/themes.json is the canonical copy of the FM1 palette data. The generator
emits both SwiftUI colors and ADV RGB565 constants; `--check` detects drift.
The default is index 24 (Desert Dusk), with all paired accent swaps intact.
The app's theme is a target-local UI preference; the ADV's is RAM-only. Neither
theme changes nor their display updates touch the audio engine or reset notes.

## Verification

- Ladder sweep remains finite through cutoff extremes at 8–192 kHz.
- Tests verify resonant peak, four-pole rolloff, seeded self-oscillation and
  integrator preservation across coefficient edits.
- Small-signal response matches the original GlitchDrums ladder within the test's
  15% RMS tolerance at low/moderate resonance; the saturation approximation is intentional.
- Native C and WASM chord/voice-steal audio match within .0002 at 32/44.1/48 kHz,
  including resonance 0, .8 and 1 and live cutoff/resonance changes.
- Six-parameter AU state and five-parameter legacy state restoration are tested.
- Theme generation checks preserve all names, RGB565 values and paired roles.
- ADV control tests confirm theme changes emit no audio commands.

Physical ADV boot/audio, CPU deadlines, headphone behavior, and iOS/DAW device
interaction remain separate checks; builds alone do not establish those results.
