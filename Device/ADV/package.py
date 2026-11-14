"""PlatformIO target: `pio run -t package` (builds, never flashes)."""
Import("env")
from pathlib import Path
import hashlib,json,shutil,subprocess,tarfile

def package(source,target,env):
    project=Path(env.subst("$PROJECT_DIR"))
    root=project.parents[1]
    build=Path(env.subst("$BUILD_DIR"))
    output=root/"build/cardputer-adv/0.10.1"
    output.mkdir(parents=True,exist_ok=True)
    entries=[]
    for offset,path in env.get("FLASH_EXTRA_IMAGES",[]):
        path=Path(env.subst(str(path)))
        number=int(str(offset),0) if not isinstance(offset,int) else offset
        entries.append((number,path))
    entries.append((int(env.subst("$ESP32_APP_OFFSET"),0),build/"firmware.bin"))
    if sorted(offset for offset,_ in entries)!=[0,0x8000,0xe000,0x10000]:
        raise RuntimeError("Unexpected ESP32-S3 flash layout; refusing package")
    args=[]
    for offset,path in sorted(entries):
        shutil.copy2(path,output/path.name)
        args.extend([hex(offset),str(path)])
    merged=output/"amsynth-adv-0.10.1-merged.bin"
    subprocess.run([env.subst("$PYTHONEXE"),env.subst("$OBJCOPY"),"--chip","esp32s3","merge_bin","-o",str(merged),"--flash_mode","keep","--flash_freq","keep","--flash_size","keep",*args],check=True)
    # Verify that every input landed at precisely its advertised flash address.
    image=merged.read_bytes()
    for offset,path in entries:
        data=path.read_bytes()
        if image[offset:offset+len(data)]!=data:raise RuntimeError("Merged image verification failed")
    source_files=sorted(p for p in project.rglob("*") if p.is_file() and ".pio" not in p.parts)
    source_files += [root/"Core/Navigation.hpp",root/"Core/Scope.hpp",root/"Firmware/firmware.h",root/"Firmware/instrument.c",root/"Firmware/ladder.h",root/"Firmware/arp.h",root/"Firmware/lfo.h",root/"Assets/themes.json",root/"Tools/generate_themes.py"]
    with tarfile.open(output/"source.tar.gz","w:gz") as archive:
        for path in source_files:archive.add(path,arcname=str(path.relative_to(root)))
    meta={"name":"AMSynth Cardputer ADV","version":"0.10.1","author":"Alex Matheu","target":"M5Stack K132-Adv / ESP32-S3FN8","hardware_tested":False,"sample_rate":32000,"block_frames":256,"merged_flash_address":"0x0","parts":[{"address":hex(offset),"file":path.name} for offset,path in sorted(entries)],"source_sha256":{str(path.relative_to(root)):hashlib.sha256(path.read_bytes()).hexdigest() for path in source_files},"toolchain":"PlatformIO espressif32 6.12.0 / Arduino-ESP32 2.0.17"}
    (output/"manifest.json").write_text(json.dumps(meta,indent=2)+"\n")
    shutil.copy2(project/"README.md",output/"README.md")
    digests=[]
    for path in sorted(output.iterdir()):
        if path.is_file() and path.name!="SHA256SUMS":digests.append(hashlib.sha256(path.read_bytes()).hexdigest()+"  "+path.name)
    (output/"SHA256SUMS").write_text("\n".join(digests)+"\n")
    print("Verified ADV package:",output)

env.AddCustomTarget(name="package",dependencies=["$BUILD_DIR/${PROGNAME}.bin"],actions=[package],title="Package Cardputer ADV firmware",description="Build and verify flash images without uploading")
