"""Exercise the resident browser runtime and compare guards to owned firmware."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class BrowserVshTest(unittest.TestCase):
    def test_asset_runtime(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ('pspkernel.h','psploadcore.h','systemctrl.h'):
                (tmp/name).write_text('#include "browser_helper_shim.h"\n')
            exe=tmp/'runtime'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-fno-pie','-no-pie','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),str(ROOT/'tests/browser_vsh_paths_test.c'),str(ROOT/'browser_local/driver/path_patch.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_owned_import_catalog(self):
        import sys
        sys.path.insert(0,str(ROOT/'tools'))
        from inspect_prx import Prx
        catalog=(ROOT/'wifi_plugin/browser_import_guards.h').read_text()
        api=Prx(ROOT/'modules/pspnet_apctl.prx',{}).report()
        exports={int(e['nid'],16):int(e['address'],16) for lib in api['exports'] for e in lib['entries']}
        sources=[ROOT/'modules'/f'{name}.prx' for name in ('htmlviewer_plugin','htmlviewer_ui','netfront')]
        sources.append(ROOT/'build/firmware/661-extracted/decrypted/F0/kd/libhttp.prx')
        for index,source in enumerate(sources):
            m=Prx(source,{})
            libraries=catalog.split(f'browser_libraries_{index}[]={{',1)[1].split('};',1)[0]
            for row in re.findall(r'\{([^}]+)\}',libraries):
                fields=row.split(',');offset,libname,flags,counts,nids,stubs=map(lambda v:int(v,16),fields[:6])
                self.assertEqual([m.word(offset+x) for x in (0,4,8,12,16)],[libname,flags,counts,nids,stubs])
                self.assertEqual(m.string(libname),fields[6].strip('"'))
            entries=catalog.split(f'browser_entries_{index}[]={{',1)[1].split('};',1)[0]
            for row in re.findall(r'\{([^}]+)\}',entries):
                fields=row.split(',');stub,nid_offset,nid,api_offset=map(lambda v:int(v,16),fields[:4])
                self.assertEqual(m.word(nid_offset),nid)
                self.assertEqual(exports[nid],api_offset)
                self.assertEqual(m.word(stub),0x03e00008)
        plugin=Prx(ROOT/'modules/htmlviewer_plugin.prx',{})
        self.assertEqual(plugin.string(0x1c534),'flash0:/vsh/resource/')
        for offset,word in ((0,0x27bdffe0),(0x133e8,0x27bdfee0),(0x18b88,0x27bdfff0)):
            self.assertEqual(plugin.word(offset),word)

    def test_owned_security_info_abi(self):
        import sys
        sys.path.insert(0,str(ROOT/'tools'))
        from inspect_prx import Prx
        api=Prx(ROOT/'modules/pspnet_apctl.prx',{})
        # Actual profile4 (AES) GetInfo mapping selects li v0,3.
        self.assertEqual(api.word(0x1199c+4*4),0x1638)
        self.assertEqual(api.word(0x163c),0x24020003)
        ui=Prx(ROOT/'modules/htmlviewer_ui.prx',{})
        # Browser connection status reads GetInfo4 and indexes its label table.
        self.assertEqual(ui.word(0x3d9fc),0x24040004)
        self.assertEqual(ui.word(0x3da24),0x00031880)
        self.assertEqual(ui.word(0x3da2c),0x8c650000)
