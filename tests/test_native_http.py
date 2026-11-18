"""Real PSP native route/TCP/HTTP client with deterministic socket/file failures."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class NativeHttpTest(unittest.TestCase):
    def test_socket_and_file_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for header in ("pspkernel.h","pspnet_inet.h"):
                (tmp/header).write_text('#include "native_http_shim.h"\n')
            exe=tmp/"native-http-test"
            subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-fsanitize=address,undefined",
                            "-I",str(tmp),"-I",str(ROOT/"tests"),"-I",str(ROOT/"common"),
                            str(ROOT/"tests/native_http_test.c"),str(ROOT/"virtual_net/app/native_http.c"),
                            str(ROOT/"common/lab_gateway.c"),"-o",str(exe)],check=True)
            for scenario in range(23):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(exe),str(scenario)],check=True,timeout=3,
                                   env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})
