#!/usr/bin/env python3
"""Package the ESP/aemu LAN candidate and its corresponding sources locally."""
import argparse
import hashlib
import ipaddress
import json
from pathlib import Path
import shutil
import subprocess
import zipfile
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,default=ROOT/'artifacts/aemu-esp-v01')
    parser.add_argument('--server',required=True,type=ipaddress.IPv4Address)
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/esp-adhoc-v01')
    args=parser.parse_args();out=args.output.resolve()
    if out.exists():raise ValueError('Use a fresh package output directory')
    provenance=json.loads((args.build/'provenance.json').read_text());tree=args.build/'source'
    backend=ROOT/'adhoc/aemu/esp_backend.c'
    if (tree/'pspnet_shims/esp_backend.c').read_bytes()!=backend.read_bytes():raise ValueError('Backend source changed since preparation')
    plugin=out/'seplugins/esp-adhoc';(plugin/'kd').mkdir(parents=True)
    shutil.copy2(tree/'dist/seplugins/atpro.prx',plugin/'atpro.prx')
    for name in ['memab','pspnet_adhoc_auth','pspnet_adhoc','pspnet_adhocctl','pspnet_adhoc_matching','pspnet_shims']:
        shutil.copy2(tree/'dist/kd'/f'{name}.prx',plugin/'kd'/f'{name}.prx')
    (plugin/'server.txt').write_text(str(args.server)+'\n')
    (plugin/'hotspot.txt').write_text('') # Existing saved profile; no SSID/password compiled in.
    (plugin/'port_offset.txt').write_text('10000\n')
    (plugin/'nickname.txt').write_text('ESP1000\n')
    (plugin/'plugins-entry.txt').write_text('game, ms0:/seplugins/esp-adhoc/atpro.prx, on\n')
    shutil.copy2(ROOT/'third_party/aemu/gpl-3.0.txt',plugin/'COPYING.txt')
    app=out/'PSP/GAME/esp-adhoc-peer';app.mkdir(parents=True)
    shutil.copy2(ROOT/'adhoc/peer_test/EBOOT.PBP',app/'EBOOT.PBP')
    (out/'BUILD.json').write_text(json.dumps(provenance,indent=2)+'\n')
    manifest={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob('*') if p.is_file()}
    (out/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(out.with_name(out.name+'-source.zip'),'w',zipfile.ZIP_DEFLATED) as z:
        files=subprocess.check_output(['git','--git-dir=.git-local','--work-tree=.','ls-files','-c','-o','--exclude-standard'],cwd=ROOT,text=True).splitlines()
        for name in files:
            p=ROOT/name
            if p.is_file():z.write(p,'esp1000/'+name)
        upstream=ROOT/'third_party/aemu'
        for p in upstream.rglob('*'):
            if p.is_file() and '.git' not in p.relative_to(upstream).parts and p.name!='.git':z.write(p,'esp1000/third_party/aemu/'+str(p.relative_to(upstream)))
    with zipfile.ZipFile(out.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as z:
        for p in out.rglob('*'):
            if p.is_file():z.write(p,str(p.relative_to(out)))
    print(out)
if __name__=='__main__':main()
