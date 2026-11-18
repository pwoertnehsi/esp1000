#!/usr/bin/env python3
"""Package the VSH prototype with the proven browser helpers; no Sony assets."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
stage = ROOT / 'artifacts/wifi-plugin'
app = stage / 'PSP/GAME/psp-browser'
plugin = stage / 'seplugins/esp-wifi'
app.mkdir(parents=True, exist_ok=True)
plugin.mkdir(parents=True, exist_ok=True)
shutil.copy2(ROOT / 'browser_local/driver/browser_paths.prx', app / 'browser_paths.prx')
shutil.copy2(ROOT / 'browser_local/offline.html', app / 'offline.html')
shutil.copy2(ROOT / 'virtual_net/driver/psp_vnet.prx', app / 'psp_vnet.prx')
shutil.copy2(ROOT / 'browser_local/app/EBOOT.PBP', app / 'EBOOT.PBP')
shutil.copy2(ROOT / 'wifi_plugin/wifi_vsh.prx', plugin / 'wifi_vsh.prx')
shutil.copy2(ROOT / 'wifi_plugin/menu_bridge/wifi_menu.prx', plugin / 'wifi_menu.prx')
for name in ['native_usb', 'native_session']:
    shutil.copy2(ROOT / 'wifi_plugin' / name / ('wifi_' + name + '.prx'), plugin / ('wifi_' + name + '.prx'))
shutil.copy2(ROOT / 'wifi_plugin/README.txt', stage / 'README.txt')
(plugin / 'plugins-entry.txt').write_text('vsh, ms0:/seplugins/esp-wifi/wifi_vsh.prx, on\nvsh, ms0:/seplugins/esp-wifi/wifi_menu.prx, on\n')
files = sorted(p for p in stage.rglob('*') if p.is_file())
manifest = {str(p.relative_to(stage)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
with zipfile.ZipFile(ROOT / 'artifacts/wifi-plugin-v21-accessory-power.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for p in files:
        archive.write(p, str(p.relative_to(stage)))
(ROOT / 'artifacts/wifi-plugin-v21-accessory-power-build.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Code-only native XMB candidate:', ROOT / 'artifacts/wifi-plugin-v21-accessory-power.zip')
