from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class LinkDiagnosticsTest(unittest.TestCase):
    def test_frame_reasons(self):
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/'diagnostics'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(ROOT/'tests/link_diagnostics_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
