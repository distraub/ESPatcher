#!/usr/bin/env python3
"""Build bounded .espkg prototype packages from human-readable JSON definitions."""
import argparse,hashlib,json,math,struct,subprocess,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIMITS=[(0,2),(.005,2),(.01,3),(80,16000),(0,.5),(0,1),(.05,20),(0,1),(0,1),(0,3),(.25,20),(.05,1),(1,4),(0,1),(40,300),(0,8),(0,1),(0,8),(0,4)]
DISCRETE={0,8,9,12,13,15,16,17,18}
def text(value,limit):
    data=value.encode('ascii')
    if not 0<len(data)<=limit or any(c<32 or c>126 for c in data):raise ValueError('Invalid package text: '+repr(value))
    return bytes([len(data)])+data
def identity(value):
    if not value or len(value)>48 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for c in value):raise ValueError('Invalid identity')
def module_manifest(data):
    position=8;manifest=None
    def integer(end):
        nonlocal position
        value=0
        for index in range(5):
            if position>=end:raise ValueError('Truncated WASM integer')
            byte=data[position];position+=1
            if index==4 and byte&240:raise ValueError('Invalid WASM integer')
            value|=(byte&127)<<(index*7)
            if not byte&128:return value
        raise ValueError('Invalid WASM integer')
    while position<len(data):
        section=data[position];position+=1;size=integer(len(data));end=position+size
        if end>len(data):raise ValueError('Truncated WASM section')
        if section==0:
            count=integer(end)
            if count>end-position:raise ValueError('Invalid custom section')
            name=data[position:position+count];position+=count
            if name==b'gs5':
                if manifest is not None:raise ValueError('Duplicate module manifest')
                manifest=json.loads(data[position:end])
        position=end
    if not isinstance(manifest,dict):raise ValueError('Missing module manifest')
    return manifest
def pack(definition,base):
    identity(definition['id']);out=bytearray(b'ESPKG001\0\0\0\0')
    out+=text(definition['id'],48)+text(definition['name'],48)+text(definition['version'],16)
    engines=definition['engines']
    if not 1<=len(engines)<=4 or len({e['id'] for e in engines})!=len(engines):raise ValueError('Invalid engines')
    out.append(len(engines))
    for engine in engines:
        identity(engine['id']);memory=engine['memory_bytes']
        if memory not in (65536,131072,262144):raise ValueError('Unsupported memory')
        out+=text(engine['id'],48)+text(engine['name'],48)+struct.pack('<I',memory)
        values=engine['defaults']
        if len(values)!=19:raise ValueError('Need 19 defaults')
        for address,(value,(lo,hi)) in enumerate(zip(values,LIMITS)):
            if not math.isfinite(value) or not lo<=value<=hi or (address in DISCRETE and int(value)!=value):raise ValueError('Invalid default')
            out+=struct.pack('<f',value)
        if values[8]!=0:raise ValueError('Transport must default to stopped')
        scenes=engine['scenes']
        if not 1<=len(scenes)<=4:raise ValueError('Need 1-4 scenes')
        out.append(len(scenes))
        for scene in scenes:
            out+=text(scene['title'],24)+text(scene['subtitle'],48)
            layout=['scope','lfo','values','levels'].index(scene['layout']);out.append(layout)
            bindings=scene['bindings'];labels=scene['labels'];pads=scene['pads']
            if len(bindings)!=8 or len(labels)!=8 or len(pads)!=16:raise ValueError('Need 8 knobs and 16 pads')
            if any(type(v)!=int or v not in range(-1,19) for v in bindings):raise ValueError('Invalid binding')
            if any(type(v)!=int or v not in range(49) for v in pads):raise ValueError('Invalid pad offset')
            out+=bytes(255 if b<0 else b for b in bindings)
            for label in labels:out+=text(label,8)
            out+=bytes(pads)
        module=(base/engine['module']).read_bytes()
        if len(module)>1048576 or not module.startswith(b'\0asm\1\0\0\0'):raise ValueError('Invalid module')
        manifest=module_manifest(module)
        if manifest.get('id')!=engine['id'] or manifest.get('memory_bytes')!=memory or manifest.get('format')!=1 or manifest.get('abi')!=1 or manifest.get('parameter_profile')!='synth-v6':raise ValueError('Package and module manifest disagree')
        out+=struct.pack('<I',len(module))+hashlib.sha256(module).digest()+module
    if len(out)>4194304:raise ValueError('Oversized package')
    struct.pack_into('<I',out,8,len(out));return out

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--rebuild-modules',action='store_true');args=parser.parse_args()
    if args.rebuild_modules:
        sdk=Path(os.environ.get('WASI_SDK_PATH','/private/tmp/wasi-sdk-25.0-arm64-macos'))
        output=ROOT/'Library/Modules';output.mkdir(parents=True,exist_ok=True)
        # Smaller experimental guest memory; desktop ABI defaults are unchanged.
        for variant,identifier,source in [(0,'pocket_analog','Firmware/instrument.c'),(1,'fm_bell','Firmware/instrument.c'),(0,'hello_tone','Firmware/Examples/hello.c'),(1,'hello_pulse','Firmware/Examples/hello.c')]:
            module=output/(identifier+'.gsm5')
            exports=['gs_version','gs_id','gs_init','gs_note','gs_parameter','gs_panic','gs_render','gs_voice_limit','gs_active_voices','gs_arp_status','gs_lfo_status']
            command=[str(sdk/'bin/clang'),'-O2','-ffp-contract=off','-mexec-model=reactor',f'-DINSTRUMENT={variant}',str(ROOT/source),'-lm','-Wl,--initial-memory=65536','-Wl,--max-memory=65536','-Wl,-z,stack-size=16384','-o',str(module)]+['-Wl,--export='+e for e in exports]
            subprocess.run(command,check=True)
            def leb(value):
                data=bytearray()
                while True:
                    byte=value&127;value>>=7;data.append(byte|(128 if value else 0))
                    if not value:return bytes(data)
            manifest={'format':1,'abi':1,'id':identifier,'name':{'pocket_analog':'AMSynth Analog','fm_bell':'AMSynth FM','hello_tone':'Hello Tone','hello_pulse':'Hello Pulse'}[identifier],'version':'0.7.2-sd1','polyphony':4,'author':'Alex Matheu','memory_bytes':65536,'target':'cardputer-adv','parameter_profile':'synth-v6','knob_labels':['CUTOFF','RES','WAVE','ATTACK','RELEASE','LEVEL','RATE','DEPTH']}
            payload=leb(3)+b'gs5'+json.dumps(manifest,separators=(',',':')).encode();module.write_bytes(module.read_bytes()+b'\0'+leb(len(payload))+payload)
    catalog={'format':1,'packages':[],'trusted_module_sha256':[]}
    for source in sorted((ROOT/'Library/Definitions').glob('*.json')):
        definition=json.loads(source.read_text());data=pack(definition,source.parent);destination=ROOT/'Library'/(definition['id']+'.espkg');destination.write_bytes(data)
        catalog['packages'].append({'id':definition['id'],'name':definition['name'],'file':destination.name,'sha256':hashlib.sha256(data).hexdigest()})
        for engine in definition['engines']:
            catalog['trusted_module_sha256'].append(hashlib.sha256((source.parent/engine['module']).read_bytes()).hexdigest())
        print(destination.name,len(data),'bytes')
    catalog['trusted_module_sha256']=sorted(set(catalog['trusted_module_sha256']))
    (ROOT/'Library/catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
if __name__=='__main__':main()
