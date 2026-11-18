from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class WifiPluginTest(unittest.TestCase):
    def test_actual_plugin(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            for header in ('pspkernel.h','pspctrl.h','pspinit.h','psploadcore.h','psploadexec_kernel.h','pspsysmem_kernel.h','pspusb.h','systemctrl.h','pspsysclib.h'):
                (tmp / header).write_text('#include "wifi_plugin_shim.h"\n')
            exe = tmp / 'plugin-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast',
                            '-fno-pie','-no-pie','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),
                            str(ROOT/'tests/wifi_plugin_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
