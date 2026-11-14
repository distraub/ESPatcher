# Contributing to ESPatcher

Start with [Getting started](docs/GETTING-STARTED.md) and the
[authoring contract](docs/FIRMWARE-AUTHORING.md). Original project code is MIT
licensed; retain dependency notices and identify the source of borrowed code.

Keep parameter addresses, engine IDs, bundle identifiers and AU component codes
stable. New engines need distinct IDs. Document any state-format migration.

Audio callbacks must not allocate, lock, log, access files, or call Swift/Obj-C.
Use the existing narrow parameter/note bridges and sample-based DSP timing.
UI animation must not act as the musical clock.

For an audio or module change, run `sh Tools/test.sh` and rebuild native/WASM
variants as needed. Build both Apple schemes when changing shared UI or bridge
code. Use `project.yml` and regenerate with XcodeGen after adding source files.
For ADV changes, run `pio run -d Device/ADV -t package` and state whether the
change was actually tried on hardware. Never describe a successful cross-build
as a hardware test.

PRs should explain the concrete behavior change, verification performed and
remaining limitations. Keep unrelated formatting and refactors separate.
