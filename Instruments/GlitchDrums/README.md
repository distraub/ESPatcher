# GlitchDrums full-firmware port

GPL-3.0-only. The independently loaded instrument is not relicensed by ESPatcher's MIT license.
See LICENSE, upstream/LICENSING.md, UPSTREAM.json and PORT-CHANGES.patch.

The complete committed 0.17.16 firmware is the baseline, not a four-lane rewrite.
All eight lanes, Analog/Glitch polyphony, modulation, arp, sequencer, pattern bank,
presets, punch effects, global FX and original save record remain in the desktop build.
The original MIDI/USB/Bluetooth compile-time feature configuration is retained.

Open index.html through an HTTP server, enable audio, then press PLAY. The original
240 x 240 screen remains intact for A/B work. Eight encoders provide Master,
Select, Algorithm, Presets and four contextual controls. White pads expose all
sixteen step keys; the additional row provides the remaining eleven chromatic keys.
SAVE preserves the firmware's original explicit-save behavior. Export project
requests an immediate RAM-backed snapshot and downloads its .gdproject record.
Import restores a bounded record on the next boot. The firmware validates its
own record format and falls back to defaults if it is invalid. Do not assume
an arbitrary file is valid just because its byte count fits.

This prototype has its own full-instrument bridge. It does not masquerade as a
19-parameter synth-v6 .gsm5 or .espkg; the existing loader limits are unchanged.
Native Apple/AUv3 embedding and the on-device SD loader integration remain open.
The Library links explicitly identify it as a browser prototype.

Build and test:

    python3 Tools/build_glitchdrums.py
    build/glitchdrums/benchmark build/glitchdrums/stock.pcm
    node Tests/glitchdrums_port.cjs

Run these commands from the ESPatcher root. Native profiler readings include
instrumentation and operating-system scheduling. They are not FM-1/Cardputer CPU.

See ../../docs/GLITCHDRUMS-PORT.md for hardware limits and the separate voice probe.
