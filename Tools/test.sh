#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 Tools/generate_themes.py --check
mkdir -p build/tests/wasm3
for name in bind code compile core emit env exec function info module optimize parse; do
    clang -std=gnu11 -O3 -DNASSERTS -UDEBUG -fomit-frame-pointer -IVendor/wasm3 -c "Vendor/wasm3/m3_$name.c" -o "build/tests/wasm3/m3_$name.o"
done
clang -std=gnu11 -O3 -DNASSERTS -UDEBUG -IVendor/wasm3 -c Vendor/wasm3/gs_sandbox.c -o build/tests/wasm3/gs_sandbox.o
clang++ -std=c++20 -O2 Tests/core.cpp -o build/tests/core
build/tests/core
clang++ -std=c++20 -O2 Tests/firmware.cpp build/tests/wasm3/*.o -o build/tests/firmware
build/tests/firmware
for variant in 0 1; do
    if [ "$variant" = 0 ]; then prefix=analog; else prefix=bell; fi
    clang -O2 -ffp-contract=off "-DINSTRUMENT=$variant" \
        "-Dgs_lfo_status=${prefix}_lfo_status" "-Dgs_arp_status=${prefix}_arp_status" "-Dgs_voice_limit=${prefix}_voice_limit" "-Dgs_active_voices=${prefix}_active_voices" "-Dgs_init=${prefix}_init" "-Dgs_note=${prefix}_note" \
        "-Dgs_render=${prefix}_render" "-Dgs_parameter=${prefix}_parameter" \
        "-Dgs_panic=${prefix}_panic" "-Dgs_id=${prefix}_id" "-Dgs_version=${prefix}_version" \
        -c Firmware/instrument.c -o "build/tests/$prefix.o"
done
clang++ -std=c++20 -O2 Tests/parity.cpp build/tests/wasm3/*.o build/tests/analog.o build/tests/bell.o -o build/tests/parity
build/tests/parity
clang++ -std=c++20 -O2 Tests/polyphony.cpp build/tests/analog.o build/tests/bell.o -o build/tests/polyphony
build/tests/polyphony
clang++ -std=c++20 -O2 -fobjc-arc -fblocks Tests/audio_unit.mm Apple/Shared/SynthAudioUnit.mm \
    build/tests/wasm3/*.o -framework AudioToolbox -framework AVFoundation -framework Foundation -o build/tests/audio_unit
build/tests/audio_unit

clang++ -std=c++17 -O2 Device/ADV/tests/controls.cpp -o build/tests/adv_controls
build/tests/adv_controls

clang++ -std=c++20 -O2 -ffp-contract=off Tests/ladder.cpp -o build/tests/ladder
build/tests/ladder

clang++ -std=c++20 -O2 Tests/lfo.cpp build/tests/analog.o -o build/tests/lfo
build/tests/lfo

clang++ -std=c++20 -O2 Tests/scope.cpp -o build/tests/scope
build/tests/scope

clang++ -std=c++20 -O2 Tests/navigation.cpp -o build/tests/navigation
build/tests/navigation

clang++ -std=c++20 -O2 Tests/arp.cpp -o build/tests/arp
build/tests/arp
clang++ -std=c++20 -O2 Tests/arp_audio.cpp build/tests/analog.o -o build/tests/arp_audio
build/tests/arp_audio

clang++ -std=c++20 -O2 Tests/lfo_shapes.cpp -o build/tests/lfo_shapes
build/tests/lfo_shapes
