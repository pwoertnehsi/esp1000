#!/usr/bin/env python3
"""Package only our browser launcher/helper; Sony assets stay in private staging."""
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
with zipfile.ZipFile(ROOT/'artifacts/browser-test-code.zip','w',zipfile.ZIP_DEFLATED) as archive:
    for source in ['browser_local/app/EBOOT.PBP','browser_local/driver/browser_paths.prx','browser_local/README.txt','browser_local/offline.html','virtual_net/driver/psp_vnet.prx']:
        p=ROOT/source
        archive.write(p,'PSP/GAME/psp-browser/'+p.name)
print('Code-only package: artifacts/browser-test-code.zip')
