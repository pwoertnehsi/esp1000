from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class UsbHostRecoveryTest(unittest.TestCase):
    def test_enumeration_policy(self):
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/'recovery'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(ROOT/'tests/usb_host_recovery_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
