#!/usr/bin/env python3
"""Apply an aemu compatibility profile to one title, preserving global settings."""
import argparse
from datetime import datetime,timezone
from pathlib import Path
import os
import re

ROOT=Path(__file__).resolve().parents[1]

def write_in_place(path,data):
    with path.open('r+b') as stream:
        stream.write(data);stream.truncate();stream.flush();os.fsync(stream.fileno())
        os.posix_fadvise(stream.fileno(),0,0,os.POSIX_FADV_DONTNEED)
    if path.read_bytes()!=data:raise OSError(f'Readback mismatch: {path}')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--psp',type=Path,required=True)
    parser.add_argument('--game',required=True)
    parser.add_argument('--ark-settings',default='PSP/SAVEDATA/ARK_01234/SETTINGS.TXT')
    args=parser.parse_args()
    if not re.fullmatch('[A-Z0-9]{9}',args.game):raise ValueError('Expected a nine-character title ID')
    root=args.psp.resolve();settings=(root/args.ark_settings).resolve()
    if not settings.is_relative_to(root):raise ValueError('Settings must be on the selected PSP')
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup=ROOT/'artifacts'/f'adhoc-game-{args.game}-{stamp}';backup.mkdir(parents=True)
    files=[(settings,[f'{args.game}, highmem:auto, on',f'{args.game}, mscache, off',f'{args.game}, infernocache, off']),
           (root/'seplugins/PLUGINS.TXT',[f'{args.game}, ms0:/seplugins/esp-adhoc/atpro.prx, on'])]
    for path,lines in files:
        data=path.read_bytes();(backup/path.name).write_bytes(data)
        missing=[line.encode() for line in lines if line.encode() not in data.splitlines()]
        if missing:
            data=data.rstrip()+b'\n# ESP1000 Ad Hoc title compatibility\n'+b'\n'.join(missing)+b'\n'
            write_in_place(path,data)
    print(f'Applied title-only Ad Hoc settings for {args.game}. Backups: {backup}')
if __name__=='__main__':main()
