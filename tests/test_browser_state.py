from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class BrowserStateTest(unittest.TestCase):
    def test_close_and_state_guards(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for header in ['pspkernel.h','psploadcore.h','kubridge.h']:
                (tmp/header).write_text('#include "browser_state_shim.h"\n')
            exe=tmp/'state-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),str(ROOT/'tests/browser_state_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
