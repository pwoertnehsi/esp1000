"""Native socket probe: mocked PSP socket operations, binary payload and cleanup failures."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeSocketTest(unittest.TestCase):
    def test_loopback_and_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            for name in ("pspnet.h", "pspnet_inet.h"):
                (tmp / name).write_text('#include "native_socket_shim.h"\n')
            executable = tmp / "native-test"
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-I", str(tmp), "-I", str(ROOT / "tests"),
                            str(ROOT / "tests/native_socket_test.c"),
                            str(ROOT / "net_probe/native_check.c"), "-o", str(executable)], check=True)
            for scenario in range(14):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(executable), str(scenario)], check=True, timeout=3)
