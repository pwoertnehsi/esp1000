import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AdhocPowerTest(unittest.TestCase):
    def test_lease(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['pspkernel.h','pspsdk.h','pspinit.h','psploadcore.h','pspusb.h','psputilsforkernel.h','pspsysmem_kernel.h','systemctrl.h']:
                (tmp/name).write_text('#include "adhoc_power_shim.h"\n')
            exe=tmp/'power-test'
            subprocess.run(['cc','-std=gnu99','-Wall','-Wextra','-Werror','-no-pie','-fsanitize=address,undefined','-I'+str(tmp),'-I'+str(ROOT/'tests'),str(ROOT/'tests/adhoc_power_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
