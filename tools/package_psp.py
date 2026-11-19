#!/usr/bin/env python3
"""Package already-built native VSH and GAME modules; Make owns compilation."""
import argparse
import hashlib
import ipaddress
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
AEMU_COMPONENTS = ('atpro', 'memab', 'pspnet_adhoc_auth', 'pspnet_adhoc',
                   'pspnet_adhocctl', 'pspnet_adhoc_matching', 'pspnet_shims')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=('production', 'debug'), default='production')
    parser.add_argument('--output', type=Path, default=ROOT/'build/production')
    parser.add_argument('--server', type=ipaddress.IPv4Address,
                        help='Optional PPSSPP server for a first installation; installer preserves existing config')
    args = parser.parse_args()
    out = args.output.resolve()
    diagnostics = int(args.profile == 'debug')
    prepared = out/'aemu-build'
    if not (prepared/'provenance.json').is_file():
        parser.error('Prepared build is missing; use make production or make debug')
    stage = out/'psp'
    wifi = stage/'seplugins/esp-wifi'
    game = stage/'seplugins/esp-adhoc'
    app = stage/'PSP/GAME/psp-browser'
    wifi.mkdir(parents=True)
    (game/'kd').mkdir(parents=True)
    app.mkdir(parents=True)
    for component, name in (('wifi_plugin', 'wifi_vsh'),
                            ('wifi_plugin/menu_bridge', 'wifi_menu'),
                            ('wifi_plugin/native_usb', 'wifi_native_usb'),
                            ('wifi_plugin/native_session', 'wifi_native_session')):
        shutil.copy2(ROOT/component/f'{name}.prx', wifi/f'{name}.prx')
    # Both the VSH helper and GAME shim use this established runtime path.
    shutil.copy2(ROOT/'virtual_net/driver/psp_vnet.prx', app/'psp_vnet.prx')
    tree = prepared/'source'
    shutil.copy2(tree/'dist/seplugins/atpro.prx', game/'atpro.prx')
    for component in AEMU_COMPONENTS[1:]:
        shutil.copy2(tree/'dist/kd'/f'{component}.prx', game/'kd'/f'{component}.prx')
    shutil.copy2(ROOT/'third_party/aemu/gpl-3.0.txt', game/'COPYING.txt')
    for name, value in {'port_offset.txt': '10000\n', 'nickname.txt': 'ESP1000\n',
                        'hotspot.txt': ''}.items():
        (game/name).write_text(value)
    if args.server:
        (game/'server.txt').write_text(f'{args.server}\n')
    (stage/'plugins-entry.txt').write_text(
        'vsh, ms0:/seplugins/esp-wifi/wifi_vsh.prx, on\n'
        'vsh, ms0:/seplugins/esp-wifi/wifi_menu.prx, on\n'
        'game, ms0:/seplugins/esp-adhoc/atpro.prx, on\n')
    if not diagnostics:
        forbidden = (b'ms0:/wifi-vsh.log', b'ms0:/native-wifi.log',
                     b'ms0:/browser-exit.log', b'ms0:/browser-thread.log',
                     b'ms0:/browser-sony-thread.log', b'ms0:/ssl-offload.log',
                     b'ms0:/ssl-capture.log', b'ms0:/psp-vnet.log',
                     b'ms0:/esp-adhoc.log', b'ms0:/esp-adhoc-power.log',
                     b'ms0:/esp-adhoc-plugin.log', b'ms0:/wifi-power.log',
                     b'ESPBrowserWatch', b'VirtualNetLog', b'atpro_input')
        for path in stage.rglob('*.prx'):
            data = path.read_bytes()
            if any(value in data for value in forbidden):
                raise ValueError(f'Production diagnostic code remains: {path}')
    (stage/'BUILD.json').write_text(json.dumps({
        'profile': args.profile, 'diagnostics': bool(diagnostics),
        'dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
        'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'aemu': json.loads((prepared/'provenance.json').read_text()),
        'files': {str(p.relative_to(stage)): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in sorted(stage.rglob('*')) if p.is_file()},
    }, indent=2)+'\n')
    with zipfile.ZipFile(out/f'esp1000-psp-{args.profile}.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                archive.write(path, str(path.relative_to(stage)))
    # Supply the actual adapted GPL sources plus project build/patch sources.
    with zipfile.ZipFile(out/'esp1000-source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        paths = subprocess.check_output(['git', 'ls-files', '-c', '-o', '--exclude-standard'],
                                        cwd=ROOT, text=True).splitlines()
        for name in sorted(set(paths)):
            path = ROOT/name
            if path.is_file():
                archive.write(path, 'esp1000/'+name)
        for path in sorted(tree.rglob('*')):
            if path.is_file() and path.suffix not in ('.o', '.elf', '.prx', '.a', '.PBP', '.SFO'):
                archive.write(path, 'adapted-aemu/'+str(path.relative_to(tree)))
        for path in sorted((prepared/'procfw').rglob('*')):
            if path.is_file():
                archive.write(path, 'procfw/'+str(path.relative_to(prepared/'procfw')))
    print(f'PSP package: {stage}')


if __name__ == '__main__':
    main()
