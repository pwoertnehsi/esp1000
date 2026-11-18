"""Exercise the actual read-only browser filesystem against mocked kernel IO."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class BrowserAssetDeviceTest(unittest.TestCase):
    def test_device(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for header in ['pspkernel.h','pspiofilemgr_kernel.h']:
                (tmp/header).write_text('#include "browser_asset_shim.h"\n')
            exe=tmp/'asset-device-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),str(ROOT/'tests/browser_asset_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
