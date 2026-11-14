# Four-voice proof of concept

Pocket Analog and FM Bell module version 0.2.0, ADV firmware candidate 0.3.0.

## What changed

- Four fixed voices per instrument. Each has oscillator phase, envelope,
  velocity, filter memory and (for Bell) modulation/decay state.
- Notes allocate an idle voice, then a quiet release tail, then the oldest held
  voice. Retriggering the same held pitch reuses its voice. Stealing has a
  32-sample transition; note-off for a stolen pitch cannot release its replacement.
- Fixed 6 dB mix headroom avoids gain changes as other notes release. A gentle
  peak bound above .75 keeps extreme chords below .9. Single notes are quieter
  than the mono prototype; use the Volume control rather than per-note normalization.
- Float sample-loop arithmetic, a 1024-entry interpolated sine table, and shared
  block coefficients replace double phase math, fmod/floor and repeated sine calls.
  Inactive voices are skipped; fully idle blocks return zero immediately.
- State uses fixed storage. No render-time allocation, file access or locks.
- ADV UI/keyboard run on core 0; DSP/playback run on core 1. No PSRAM required.
- Optional ABI 1 voice-count exports preserve compatibility with older modules.

The original browser study and legacy native fallback remain monophonic. The two
current importable modules and their native ADV builds are four-voice instruments.

## Horsepower: what is known

The ESP32-S3 has two LX7 cores up to 240 MHz and a single-precision FPU:
https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf

This implementation is intended to fit four simple voices, not to establish a
maximum polyphony from the CPU specification. The workload depends on oscillator,
filter, modulation, sample rate, driver overhead, and UI/cache contention. DSP
currently runs on one core, alongside the playback task. A Mac benchmark does
not establish ESP32 timing. Physical keyboard rollover is also unverified.

ADV linker report: **66,988 bytes static RAM / 327,680 bytes**, about 8 KB more
than the mono build; approximately 541 KB flash. Runtime task/driver heap is
additional. The extra static storage mostly holds sine tables and voice state.
The instrument object no longer references double-precision math helpers.

## Test on arrival

The 32 kHz / 256-frame block deadline is **8000 µs**. Press **8** on the ADV to
toggle a repeating C–E–G–B test chord and open diagnostics, bypassing keyboard
rollover. `Voices` should show 4 during the test. It retriggers every second so
Bell's natural decay does not turn the test into an idle measurement.

Aim for max DSP time **under 4000 µs** as an initial margin target, with no rising
late-block count, while changing controls and displaying diagnostics. This is a
target, not an observed result. `Q gap` counts an empty software queue, including
startup, and is not a hardware DMA underrun measurement. CPU, actual codec clock,
latency, output quality and sustained stability require the physical board.

Use the Mac app's **Test chord** button (or key 8) to audition four notes. That
chord holds until released; the hardware version retriggers for CPU testing.

## Verification

- Spectral test detects all four simultaneous pitches at 32/44.1/48 kHz.
- Confirms oldest-held stealing, released-tail preference and stolen-note-off isolation.
- All 128 MIDI notes and all shapes remain finite and bounded; release and panic reach idle.
- Native C/WASM chord and stealing audio agree within 0.0002 at all three rates.
- AU test confirms four new notes queued after panic survive in order.
- Existing mono module loading, fault handling and state-restore tests remain covered.
- ADV four-note control event and repeat timing tests pass.

The source allows 1–8 voices for native experiments, but current modules and
firmware are fixed at four. Increasing that limit should follow device measurements.
