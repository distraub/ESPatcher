# GlitchDrums full-port baseline

The first port preserves GlitchDrums FM1 0.17.16 at commit
`4256d0b5e230862156038beb19ad41fcec2b3fd2`. It is a full-feature desktop/browser
baseline, not yet a production Cardputer instrument or an Apple AUv3 engine.

## Reproducible comparison

`python3 Tools/build_glitchdrums.py` builds the native baseline/profiler and a
freestanding WASM module with zero imports and a fixed 2 MiB memory reservation.
The old RemoteFM1 wrapper's 8 MiB reservation is not carried over. The full
instrument is intentionally separate from the current bounded synth ABI; none
of ESPatcher's .gsm5/.espkg memory/fuel limits were relaxed for this port.

The test renders 441,088 stereo frames of the factory sequence in both native
and WASM builds and compares every signed PCM sample exactly. It additionally
checks project export/restore, isolated instances, screen navigation and key
release. This proves baseline parity for that fixture, not every possible project.

The native profile uses the same per-lane/control/hit/FX hooks as FM-1. Independent
stage peaks must not be added. The desktop per-stage profiler uses thread CPU time, while outer block/wall-time
peaks can include scheduling preemption; do not translate them into embedded speedups. Instrumentation has
its own cost. All firmware state is single-owner; this snapshot retains the
original serialized API contract and must not be called concurrently.

## Cardputer build results

The unchanged full native target, `glitchdrums_full`, compiles but cannot link:
its static state and original framebuffer overflow `dram0_0_seg` by **154,760
bytes** with the pinned ESP32 Arduino/M5 toolchain. A headless full-firmware
experiment still overflowed by 25,584 bytes. No full instrument binary is shipped,
and no Cardputer has been flashed or timed. The original 240 x 240 framebuffer
also needs an explicit 240 x 135 workflow adaptation rather than silent cropping.

These are actual linker results, not a CPU conclusion. The DSP may fit its audio
deadline once the memory architecture is addressed; that remains unmeasured.
Full desktop features and sound have not been reduced to hide the device limit.
The experimental full target is retained for an objective memory baseline.

## Buildable voice-engine CPU probe

    /private/tmp/gs-pio/bin/pio run -d Device/ADV/GlitchDrums -e glitchdrums_cpu

Use your own `pio` installation if the temporary environment does not exist.
The default environment builds the **voice-only probe**, not the full instrument.
It retains all eight original voice engines and 14 voice slots. It does not
include the original sequencer, capture/punch/global FX, display, SD persistence,
or I2S playback. Factory step values drive a simple fixed-120-BPM trigger fixture;
this is deliberately a focused processor measurement, not full-firmware parity.
Do not present its measurements as total playback CPU or underrun performance.

Cross-build result with profiling enabled: 38,664 bytes static RAM and 497,073
bytes flash. Rebuild output is authoritative for subsequent changes. At 44.1 kHz,
a 256-frame slice has about 5,805 microseconds. Serial output reports mean/max
voice-render time, heap and stack, plus independent stage peaks when profiling
is enabled. Measurement is opt-in: no upload command runs as part of the build.

## Next integration work

1. Collect actual Cardputer voice-probe timings with the same quality/fixture.
2. Replace the copied full framebuffer and RAM-backed flash emulator with native
   display and SD services; budget the three independent 64 KiB effect histories
   and cold editor/project state explicitly.
3. Define a full-instrument UI/control/state ABI rather than squeezing this into
   synth-v6 parameter meanings. Add native Apple/AUv3 hosting behind that ABI.
4. Only then test native full playback, UI redraws, SD operations and effect tails
   together. Require headroom, not merely an average under the block deadline.

DSP optimizations that preserve behavior can be compared against this pinned
baseline, then reviewed separately for backporting to FM-1. This work does not
modify the actively developed FM-1 checkout.

## Licensing

The instrument, bridge, front end and dedicated tests carry GPL-3.0-only;
upstream copyright notices and Terminus font licensing are retained. ESPatcher
loads the browser instrument separately. No GPL engine is statically linked
into the MIT Apple host by this change. See Instruments/GlitchDrums/LICENSE.
