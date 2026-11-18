"""Run the actual browser APCTL projection against deterministic thread/IO mocks."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AdapterTest(unittest.TestCase):
    def test_adapter(self):
        config=shutil.which('psp-config')
        if not config:self.skipTest('PSPSDK unavailable')
        sdk=Path(subprocess.check_output([config,'--pspsdk-path'],text=True).strip())/'include'
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            (tmp/'pspkernel.h').write_text('#include "browser_adapter_shim.h"\n')
            (tmp/'pspnet.h').write_text('#include "browser_adapter_shim.h"\n')
            shutil.copyfile(sdk/'pspnet_apctl.h',tmp/'pspnet_apctl.h')
            exe=tmp/'adapter-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),str(ROOT/'tests/browser_adapter_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
