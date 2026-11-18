"""Exercise actual DNS worker lifetime and timeout code with a deterministic scheduler."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class BrowserLookupTest(unittest.TestCase):
    def test_lookup(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['pspkernel.h','pspnet.h','pspnet_resolver.h']:
                (tmp/name).write_text('#include "browser_lookup_shim.h"\n')
            exe=tmp/'lookup-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),str(ROOT/'tests/browser_lookup_test.c'),'-o',str(exe)],check=True)
            for mode in range(9):
                with self.subTest(mode=mode):
                    subprocess.run([str(exe),str(mode)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
