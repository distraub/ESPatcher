#!/usr/bin/env python3
"""Build the full GPL instrument independently of the MIT ESPatcher host."""
import hashlib,json,os,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
P=R/'Instruments/GlitchDrums'; O=R/'build/glitchdrums'; O.mkdir(parents=True,exist_ok=True)
inc=['-I'+str(P/'upstream'/s) for s in ['platform','platform/src','platform/gen']]
sources=[str(P/'port/bridge.c'),str(P/'port/firmware.c')]
base=['-std=c11','-O2','-fno-builtin','-DFOMA_MVAVE=1','-DFOMA_MIDI_CLOCK=0','-DFOMA_USB_AUDIO=0','-DFOMA_BLUETOOTH=0']+inc
for source in sources:
    subprocess.run(['clang']+base+['-DGD_PORT_NATIVE=1','-DGD_PORT_PROFILE=1','-c',source,'-o',str(O/(Path(source).stem+'.o'))],check=True)
subprocess.run(['clang++','-std=c++20','-O2',str(R/'Tests/glitchdrums_port.cpp'),str(O/'bridge.o'),str(O/'firmware.o'),'-o',str(O/'benchmark')],check=True)
sdk=Path(os.environ.get('WASI_SDK_PATH','/private/tmp/wasi-sdk-25.0-arm64-macos'))
wasm=P/'glitchdrums.wasm'
subprocess.run([str(sdk/'bin/clang')]+base+['-nostdlib','-mno-bulk-memory','-mno-reference-types','-mno-multivalue','-Wno-incompatible-library-redeclaration','-Wl,--no-entry','-Wl,--export-memory','-Wl,-z,stack-size=65536','-Wl,--initial-memory=2097152','-Wl,--max-memory=2097152']+sources+['-o',str(wasm)],check=True)
upstream=json.loads((P/'UPSTREAM.json').read_text())
manifest={'format':'espatcher-full-instrument-prototype-1','id':'glitchdrums','name':'GlitchDrums','license':'GPL-3.0-only','upstream':upstream,'sample_rate':44100,'memory_bytes':2097152,'screen':[240,240],'module_sha256':hashlib.sha256(wasm.read_bytes()).hexdigest(),'module_bytes':wasm.stat().st_size,'reduced_features':[],'device_timing':'unmeasured','compatibility':'Dedicated full-instrument prototype; not a .gsm5/.espkg synth-profile module'}
manifest['source_sha256']={str(f.relative_to(P)):hashlib.sha256(f.read_bytes()).hexdigest() for directory in ['upstream','port'] for f in sorted((P/directory).rglob('*')) if f.is_file()}
(P/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
