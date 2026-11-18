from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class BrowserRuntimeTest(unittest.TestCase):
    def test_log_and_trace(self):
        with tempfile.TemporaryDirectory() as directory:
            exe=Path(directory)/'runtime-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(ROOT/'tests/browser_runtime_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
