#!/usr/bin/env python3
"""Extract a verified Sony 6.61 update locally; never installs or flashes it.

Requires pspdecrypt. Output stays separate from modules/ and release packages.
ARK installer executables and installer archives are not Sony firmware archives.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import struct
import subprocess
import shutil
import os
import sys
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_INPUT = ROOT / 'build/firmware/661.PBP'
LOCAL_DECRYPTOR = ROOT / 'third_party/pspdecrypt/pspdecrypt'

MAX_BYTES = 64 * 1024 * 1024
SONY_661_MD5 = '6be8878f475ac5b1a499b95ab2f7d301'
SONY_661_URL = ('http://du01.psp.update.playstation.org/update/psp/image/us/'
                '2014_1212_6be8878f475ac5b1a499b95ab2f7d301/EBOOT.PBP')
PARTS = ('PARAM.SFO', 'ICON0.PNG', 'ICON1.PMF', 'PIC0.PNG', 'PIC1.PNG',
         'SND0.AT3', 'DATA.PSP', 'DATA.PSAR')
TARGETS = ('htmlviewer_ui.prx', 'netfront.prx', 'htmlviewer.res')


def inspect(data):
    if data[:4] == b'\x7fELF':
        return {'kind': 'executable', 'message': 'ELF executable, not a firmware archive'}
    if len(data) < 40 or data[:4] != b'\0PBP':
        raise ValueError('Expected a PBP package or ELF executable')
    offsets = list(struct.unpack_from('<8I', data, 8)) + [len(data)]
    if offsets[0] < 40 or any(a > b for a, b in zip(offsets, offsets[1:])):
        raise ValueError('Invalid PBP section boundaries')
    result = {'kind': 'pbp', 'parts': [
        {'name': name, 'offset': a, 'bytes': b - a}
        for name, a, b in zip(PARTS, offsets, offsets[1:])]}
    psar = data[offsets[7]:]
    if psar.startswith(b'PSAR'):
        result['archive'] = 'Sony PSAR'
    elif len(psar) >= 4:
        # ARK's simple count / size / name-length / name / content archive.
        count, = struct.unpack_from('<I', psar)
        pos, entries = 4, []
        if not 0 < count <= 512:
            result['archive'] = 'unknown'
            return result
        for _ in range(count):
            if pos + 5 > len(psar):
                result['archive'] = 'unknown'
                return result
            size, name_len = struct.unpack_from('<IB', psar, pos)
            pos += 5
            if not name_len or pos + name_len + size > len(psar):
                result['archive'] = 'unknown'
                return result
            name = psar[pos:pos + name_len].decode('ascii', errors='replace')
            pos += name_len
            entries.append({'name': name, 'bytes': size})
            pos += size
        if pos != len(psar):
            result['archive'] = 'unknown'
            return result
        result['archive'] = 'ARK installer bundle'
        result['entries'] = entries
    else:
        result['archive'] = 'empty'
    return result


def download(url, dest):
    """Fetch the update, verify its MD5, then move to target location."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix('.part')
    md5, size = hashlib.md5(), 0
    try:
        with urllib.request.urlopen(url, timeout=30) as response, tmp.open('wb') as out:
            while chunk := response.read(1 << 20):
                size += len(chunk)
                if size > MAX_BYTES:
                    raise ValueError('Download exceeds the expected firmware size limit')
                md5.update(chunk)
                out.write(chunk)
        if md5.hexdigest() != SONY_661_MD5:
            raise ValueError('Downloaded file does not match the expected 6.61 MD5')
        tmp.replace(dest)
    finally:
        tmp.unlink(missing_ok=True)

def extract(source, output, decryptor):
    if source.stat().st_size > 64 * 1024 * 1024:
        raise ValueError('Input exceeds the expected firmware size limit')
    data = source.read_bytes()
    info = inspect(data)
    if info.get('archive') != 'Sony PSAR':
        raise ValueError(f"Not a Sony update: {info.get('archive', info['kind'])}")
    if hashlib.md5(data).hexdigest() != SONY_661_MD5:
        raise ValueError('Input does not match the expected standard PSP 6.61 update MD5')
    output.mkdir(parents=True, exist_ok=False)
    dump = output / 'decrypted'
    with (output / 'extraction.log').open('w') as log:
        subprocess.run([str(decryptor.resolve()), '-A', '-O', str(dump.resolve()),
                        str(source.resolve())], stdout=log, stderr=subprocess.STDOUT, check=True)
    found = {}
    for name in TARGETS:
        paths = sorted(p for p in dump.rglob(name) if p.is_file())
        found[name] = [{'path': str(p.relative_to(output)), 'bytes': p.stat().st_size,
                        'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                       for p in paths]
    manifest = {'source': str(source), 'source_url': SONY_661_URL,
                'source_sha256': hashlib.sha256(data).hexdigest(),
                'source_md5': SONY_661_MD5, 'assets': found}
    (output / 'browser-assets.json').write_text(json.dumps(manifest, indent=2) + '\n')
    if not all(found.values()):
        raise ValueError('Extraction finished but browser files are missing; inspect extraction.log')
    return manifest



def stage_browser_assets(extracted, listing):
    """Stage only files omitted from the console listing, without overwrites."""
    source = json.loads((extracted / 'browser-assets.json').read_text())
    pbp_data = Path(source['source']).read_bytes()
    if (hashlib.md5(pbp_data).hexdigest() != SONY_661_MD5
            or hashlib.sha256(pbp_data).hexdigest() != source['source_sha256']):
        raise ValueError('Source update no longer matches extraction provenance')
    root = extracted / 'decrypted/F0'
    installed, directory = set(), None
    for line in listing.read_text().splitlines():
        if line.startswith('Listing directory flash0:/'):
            directory = line.split('flash0:/', 1)[1].strip().strip('/')
        elif directory:
            entry = re.match(r'^-[rwx-]{9}\s+\d+\s+\S+\s+\S+\s+(.+)$', line)
            if entry:
                installed.add(directory + '/' + entry[1])
    if 'vsh/module/htmlviewer_plugin.prx' not in installed:
        raise ValueError('Expected a complete console module listing')
    paths = ['vsh/module/' + name + '.prx' for name in
             ('htmlviewer_ui', 'netfront', 'libslim', 'libwww', 'mm_flash',
              'libfont_hv', 'hvauth_r', 'htmlviewer_utility', 'htmlviewer_plugin')]
    paths += ['vsh/resource/' + name for name in
              ('htmlviewer.res', 'htmlviewer_plugin.rco', 'netconf_dialog.rco')]
    stage = extracted.parent / 'browser-runtime'
    assets = []
    # Validate every input before creating the staging tree.
    contents = {relative: (root / relative).read_bytes() for relative in paths}
    for relative, data in contents.items():
        present = relative in installed
        target = stage / relative
        if not present and target.exists() and target.read_bytes() != data:
            raise ValueError(f'Refusing to overwrite differing asset: {target}')
        assets.append({'firmware_path': 'flash0:/' + relative,
                       'present_on_e1000': present, 'decrypted_bytes': len(data),
                       'source': str(root / relative),
                       'sha256': hashlib.sha256(data).hexdigest()})
    for relative, data in contents.items():
        target = stage / relative
        if relative not in installed and not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open('xb') as output:
                output.write(data)
    report = {'source_sha256': source['source_sha256'], 'assets': assets}
    (stage / 'assets.json').write_text(json.dumps(report, indent=2) + '\n')
    (stage / 'README.txt').write_text(
        'Browser files recovered from a verified standard Sony 6.61 update.\n'
        'This is an analysis staging directory, not a launcher or installer.\n'
        'Runtime redirection and browser operation are still untested.\n')
    return {'staging_directory': str(stage),
            'staged_files': [a['firmware_path'] for a in assets if not a['present_on_e1000']]}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, nargs='?', default=DEFAULT_INPUT)
    parser.add_argument('--download', action='store_true',
                        help='Fetch the update if the input file is missing')
    parser.add_argument('--stage-only', action='store_true',
                        help='Stage missing browser files from an existing extraction')
    parser.add_argument('--listing', type=Path, default=ROOT / 'module_listings.txt')
    parser.add_argument('--inspect', action='store_true', help='List container contents only')
    parser.add_argument('--decryptor', type=Path,
                        default=Path(os.environ.get('PSPDECRYPT')
                            or (LOCAL_DECRYPTOR if LOCAL_DECRYPTOR.exists() else None)
                            or shutil.which('pspdecrypt') or 'pspdecrypt'))
    parser.add_argument('--output', type=Path, default=ROOT / 'build/firmware/661-extracted')
    args = parser.parse_args()
    if args.download and not args.stage_only and not args.input.exists():
        download(SONY_661_URL, args.input)
    try:
        if args.stage_only:
            result = stage_browser_assets(args.output, args.listing)
        elif args.inspect:
            if args.input.stat().st_size > 64 * 1024 * 1024:
                raise ValueError('Input exceeds size limit')
            result = inspect(args.input.read_bytes())
        else:
            result = extract(args.input, args.output, args.decryptor)
            if args.listing.exists():
                result['staging'] = stage_browser_assets(args.output, args.listing)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'Firmware preparation failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
