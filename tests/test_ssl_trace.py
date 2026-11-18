import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class SslTraceTest(unittest.TestCase):
    def test_stream_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            exe=Path(directory)/'ssl-trace'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'common'),str(ROOT/'tests/ssl_trace_test.c'),str(ROOT/'common/ssl_trace.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
