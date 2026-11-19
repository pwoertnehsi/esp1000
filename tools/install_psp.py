#!/usr/bin/env python3
"""Install a verified PSP package, preserving profiles, settings and personal config."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
CONFIG_NAMES = {'server.txt', 'hotspot.txt', 'port_offset.txt', 'nickname.txt'}


def write_verified(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('r+b' if path.exists() else 'w+b') as stream:
        stream.write(data)
        stream.truncate()
        stream.flush()
        os.fsync(stream.fileno())
        os.posix_fadvise(stream.fileno(), 0, 0, os.POSIX_FADV_DONTNEED)
    if path.read_bytes() != data:
        raise OSError(f'Readback mismatch: {path}')


def install(build, psp, backup):
    manifest = json.loads((build/'BUILD.json').read_text())
    payload = []
    for name, digest in manifest['files'].items():
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError(f'Invalid package path: {name}')
        source = (build/relative).resolve()
        target = (psp/relative).resolve()
        if not source.is_relative_to(build) or not target.is_relative_to(psp):
            raise ValueError(f'Path escapes package or PSP: {name}')
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'Package checksum mismatch: {name}')
        if relative.suffix == '.prx' or (relative.parts[:2] == ('seplugins', 'esp-adhoc') and relative.name in CONFIG_NAMES | {'COPYING.txt'}):
            if relative.name in CONFIG_NAMES and target.exists():
                continue
            payload.append((relative, target, data))
    # Verify everything before writing, and back up every file being replaced.
    backup.mkdir(parents=True, exist_ok=False)
    for relative, target, _ in payload:
        if target.exists():
            saved = backup/relative
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, saved)
    plugins = psp/'seplugins/PLUGINS.TXT'
    if plugins.exists():
        shutil.copy2(plugins, backup/'PLUGINS.TXT')
    entries = (build/'plugins-entry.txt').read_bytes().splitlines()
    old = plugins.read_bytes() if plugins.exists() else b''
    # Retain title-specific GAME entries and intentionally disabled entries.
    active_lines = [line.strip().lower() for line in old.splitlines() if not line.strip().startswith(b'#')]
    missing = [line for line in entries if line.split(b',')[1].strip().lower() not in
               [part.strip() for existing in active_lines for part in existing.split(b',')[1:2]]]
    for _, target, data in payload:
        write_verified(target, data)
    if missing:
        write_verified(plugins, old.rstrip()+b'\n'+b'\n'.join(missing)+b'\n')
    print(f'Installed {manifest["profile"]} PSP modules; backup: {backup}')
    print('Safely eject, leave USB mode and restart VSH. GAME changes apply on the next game launch.')
    if not (psp/'seplugins/esp-adhoc/server.txt').exists():
        print('Set seplugins/esp-adhoc/server.txt to your PPSSPP server IPv4 before using Ad Hoc.')
    if not (psp/'b0').exists() or not (psp/'b1').exists():
        print('Native browser assets b0/b1 are missing; see docs/BUILD_INSTALL.md.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True, help='The generated psp/ directory')
    parser.add_argument('--psp', type=Path, required=True)
    args = parser.parse_args()
    psp = args.psp.resolve()
    if not os.path.ismount(psp):
        parser.error('--psp must be the mounted PSP root')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    install(args.build.resolve(), psp, ROOT/'build'/f'install-backup-{stamp}')


if __name__ == '__main__':
    main()
