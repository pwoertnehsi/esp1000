import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class TlsOffloadTest(unittest.TestCase):
    def test_psp_client(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['pspkernel.h','psploadcore.h','pspnet_inet.h','kubridge.h']:(tmp/name).write_text('#include "tls_client_shim.h"\n')
            (tmp/'netinet').mkdir();(tmp/'netinet/in.h').write_text('#include "tls_client_shim.h"\n')
            exe=tmp/'client'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-fno-pie','-no-pie','-fsanitize=address,undefined','-I'+str(tmp),'-I'+str(ROOT/'tests'),str(ROOT/'tests/tls_client_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_import_guards(self):
        with tempfile.TemporaryDirectory() as directory:
            exe=Path(directory)/'hooks'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast','-fsanitize=address,undefined',str(ROOT/'tests/tls_hooks_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_esp_service(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['freertos/FreeRTOS.h','freertos/task.h','freertos/semphr.h','lwip/sockets.h','esp_tls.h','esp_crt_bundle.h','esp_log.h','esp_netif_sntp.h']:
                path=tmp/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('#include "tls_server_shim.h"\n')
            exe=tmp/'server'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(tmp),'-I'+str(ROOT/'tests'),'-I'+str(ROOT/'common'),str(ROOT/'tests/tls_server_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
