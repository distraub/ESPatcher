#!/usr/bin/env python3
"""Build importable .gsm5 WASM modules with a local WASI SDK."""
import hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def leb(n):
    out=bytearray()
    while True:
        b=n&127;n>>=7;out.append(b|(128 if n else 0))
        if not n:return bytes(out)
sdk=Path(os.environ.get('WASI_SDK_PATH','/private/tmp/wasi-sdk-25.0-arm64-macos'))
output=ROOT/'Firmware/Modules';output.mkdir(parents=True,exist_ok=True)
entries=[(0,'pocket_analog','AMSynth Analog',0),(1,'fm_bell','AMSynth FM',0)]
if '--fixtures' in sys.argv:
    entries += [(0,'pocket_analog','Test runaway',1),(0,'pocket_analog','Test bounds',2),(0,'pocket_analog','Test trap',3)]
for variant,identifier,name,bad in entries:
    dest=output/(identifier+'.gsm5')
    if bad:
        fixture=ROOT/'Tests/Fixtures';fixture.mkdir(exist_ok=True)
        dest=fixture/(['','runaway','bounds','trap'][bad]+'.gsm5')
    exports=['gs_version','gs_id','gs_init','gs_note','gs_parameter','gs_panic','gs_render','gs_voice_limit','gs_active_voices','gs_arp_status','gs_lfo_status']
    command=[str(sdk/'bin/clang'),'-O2','-ffp-contract=off','-mexec-model=reactor','-DINSTRUMENT='+str(variant),'-DGS_TEST_BAD='+str(bad),str(ROOT/'Firmware/instrument.c'),'-lm','-Wl,--initial-memory=262144','-Wl,--max-memory=262144','-Wl,-z,stack-size=65536','-o',str(dest)]
    command += ['-Wl,--export='+x for x in exports]
    subprocess.run(command,check=True)
    manifest={'format':1,'abi':1,'id':identifier,'name':name,'version':'0.7.2','polyphony':4,'author':'Alex Matheu','memory_bytes':262144,'target':'cardputer-adv','parameter_profile':'synth-v6','knob_labels':['CUTOFF','RES','RATIO' if variant else 'WAVE','ATTACK','RELEASE','LEVEL','RATE','DEPTH']}
    payload=leb(3)+b'gs5'+json.dumps(manifest,separators=(',',':')).encode()
    dest.write_bytes(dest.read_bytes()+b'\0'+leb(len(payload))+payload)
    print(dest.name,hashlib.sha256(dest.read_bytes()).hexdigest())
