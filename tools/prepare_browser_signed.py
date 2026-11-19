#!/usr/bin/env python3
"""Verify original Sony browser modules against our known decrypted extraction.
Private local staging only. No encryption/decryption/partition checks are patched.
"""
from pathlib import Path
import gzip
import hashlib
import json
import struct
import subprocess
import shutil
import os
import tempfile
from prepare_browser_firmware import SONY_661_MD5
ROOT=Path(__file__).resolve().parents[1]
NAMES=['htmlviewer_ui.prx','netfront.prx','libslim.prx','libwww.prx','mm_flash.prx']

def signed_assets():
    firmware=ROOT/'build/firmware/661.PBP'
    if hashlib.md5(firmware.read_bytes()).hexdigest()!=SONY_661_MD5:
        raise ValueError('Firmware provenance mismatch')
    decryptor=Path(os.environ.get('PSPDECRYPT', shutil.which('pspdecrypt') or 'pspdecrypt'))
    folder=ROOT/'build/firmware/661-encrypted'
    if not folder.exists():
        with (ROOT/'build/firmware/661-encrypted-extract.log').open('w') as log:
            subprocess.run([str(decryptor),'-A','-e','-O',str(folder),str(firmware)],stdout=log,stderr=subprocess.STDOUT,check=True)
    matrix=json.loads((ROOT/'analysis/browser_asset_matrix.json').read_text())
    expected={Path(a['firmware_path']).name:a['sha256'] for a in matrix['assets'] if a['firmware_path'].endswith('.prx')}
    result={}
    with tempfile.TemporaryDirectory(prefix='browser-signed-') as directory:
        for name in NAMES:
            source=folder/'F0/vsh/module'/name;data=source.read_bytes()
            if data[:4]!=b'~PSP' or len(data)!=struct.unpack_from('<I',data,0x2c)[0]:
                raise ValueError(f'Invalid original module header: {name}')
            target=Path(directory)/name
            subprocess.run([str(decryptor),'-o',str(target),str(source)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
            decoded=target.read_bytes()
            if decoded[:2]==b'\x1f\x8b':decoded=gzip.decompress(decoded)
            if hashlib.sha256(decoded).hexdigest()!=expected[name]:
                raise ValueError(f'Original/decrypted module mismatch: {name}')
            result[name]={'source':str(source.relative_to(ROOT)),'sha256':hashlib.sha256(data).hexdigest(),
                          'bytes':len(data),'decrypted_sha256':expected[name]}
    (ROOT/'analysis/browser_signed_assets.json').write_text(json.dumps(result,indent=2)+'\n')
    return result

if __name__=='__main__':
    assets=signed_assets()
    print(f'Verified {len(assets)} original Sony modules against decrypted SHA256 references')
