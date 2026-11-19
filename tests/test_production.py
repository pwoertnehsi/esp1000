from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from tools.install_psp import install
ROOT = Path(__file__).resolve().parents[1]


class ProductionTest(unittest.TestCase):
    def test_callback_loggers_are_silent(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            for header in ('pspkernel.h', 'psploadcore.h', 'pspmodulemgr.h',
                           'pspiofilemgr.h', 'pspusb.h', 'psputility_usbmodules.h',
                           'psputility_netparam.h', 'psputility_netmodules.h',
                           'pspnet_apctl.h', 'kubridge.h'):
                (tmp/header).write_text('#include "native_adapter_shim.h"\n')
            (tmp/'quiet.c').write_text('''
#define ESP1000_DIAGNOSTICS 0
#define main developer_test_main
#include "native_adapter_test.c"
#undef main
int main(void) {
    forbid_log_io=1;
    trace_event(20,0,5);log_result(2,0,0);
    wifi_native_exit_log("exit",4);wifi_native_ssl_log("tls",3);
    wifi_native_log_drain();
    assert(!log_head && !log_tail && !log_dropped);
    return 0;
}
''')
            subprocess.run(['cc', '-std=c11', '-O2', '-fno-pie', '-no-pie',
                            '-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast',
                            '-I', str(tmp), '-I', str(ROOT/'tests'),
                            str(tmp/'quiet.c'), '-o', str(tmp/'quiet')], check=True)
            subprocess.run([str(tmp/'quiet')], check=True, timeout=5)

    def test_installer_preserves_config_and_title_entries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);build = root/'package';psp = root/'psp'
            build.mkdir();psp.mkdir()
            files = {'seplugins/esp-wifi/wifi_vsh.prx': b'new',
                     'seplugins/esp-adhoc/server.txt': b'192.0.2.1\n',
                     'plugins-entry.txt': b'vsh, ms0:/seplugins/esp-wifi/wifi_vsh.prx, on\ngame, ms0:/seplugins/esp-adhoc/atpro.prx, on\n'}
            for name, data in files.items():
                path = build/name;path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(data)
            (build/'BUILD.json').write_text(json.dumps({'profile': 'production', 'files': {
                n: hashlib.sha256(v).hexdigest() for n,v in files.items()}}))
            (psp/'seplugins/esp-adhoc').mkdir(parents=True)
            (psp/'seplugins/esp-adhoc/server.txt').write_bytes(b'keep mine\n')
            entries = b'TITLE1234, ms0:/seplugins/esp-adhoc/atpro.prx, on\n'
            (psp/'seplugins/PLUGINS.TXT').write_bytes(entries)
            (psp/'b0').mkdir();(psp/'b0/htmlviewer.res').write_bytes(b'private resource')
            (psp/'b1').mkdir();(psp/'b1/module.prx').write_bytes(b'private module')
            install(build.resolve(), psp.resolve(), root/'backup')
            self.assertFalse((psp/'b0').exists())
            self.assertEqual((psp/'PSP/GAME/psp-browser/b1/module.prx').read_bytes(), b'private module')
            self.assertEqual((psp/'seplugins/esp-adhoc/server.txt').read_bytes(), b'keep mine\n')
            text = (psp/'seplugins/PLUGINS.TXT').read_bytes()
            self.assertTrue(text.startswith(entries))
            self.assertNotIn(b'game,', text)
            self.assertIn(b'vsh,', text)
            (build/'seplugins/esp-wifi/wifi_vsh.prx').write_bytes(b'tampered')
            with self.assertRaises(ValueError):
                install(build.resolve(), psp.resolve(), root/'bad-backup')
            self.assertFalse((root/'bad-backup').exists())
