import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AdhocMapTest(unittest.TestCase):
    def test_service(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['freertos/FreeRTOS.h','lwip/udp.h','lwip/lwip_napt.h','lwip/netif.h','esp_log.h']:
                path=tmp/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('#include "adhoc_map_shim.h"\n')
            exe=tmp/'map-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(tmp),'-I'+str(ROOT/'tests'),'-I'+str(ROOT/'common'),str(ROOT/'tests/adhoc_map_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
