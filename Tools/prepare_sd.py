#!/usr/bin/env python3
"""Stage an SD-card tree; does not mount media, format, flash, or upload anything."""
import argparse,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=ROOT/'build/sd-card');args=parser.parse_args()
    destination=args.output/'ESPatcher/Instruments';destination.mkdir(parents=True,exist_ok=True)
    catalog=json.loads((ROOT/'Library/catalog.json').read_text());digests=[]
    for entry in catalog['packages']:
        source=ROOT/'Library'/entry['file'];data=source.read_bytes()
        digest=hashlib.sha256(data).hexdigest()
        if digest!=entry['sha256']:raise ValueError('Rebuild stale package catalog before staging')
        target=destination/source.name;shutil.copyfile(source,target);digests.append(digest+'  '+source.name)
    (destination/'SHA256SUMS').write_text('\n'.join(digests)+'\n')
    shutil.copyfile(ROOT/'LICENSE',args.output/'ESPatcher/LICENSE')
    print('Staged',len(digests),'packages at',destination)
    print('Copy the ESPatcher folder to your SD root. No device has been flashed.')
if __name__=='__main__':main()
