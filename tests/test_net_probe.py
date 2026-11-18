"""Real probe: module ownership, partial failures, HOME, and logging failures."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NetProbeTest(unittest.TestCase):
    def test_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            for name in ("pspkernel.h", "pspdebug.h", "pspctrl.h", "psploadcore.h",
                         "psputility_netmodules.h", "kubridge.h", "systemctrl.h"):
                (tmp / name).write_text('#include "net_probe_shim.h"\n')
            flags = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                     "-Wno-pointer-to-int-cast", "-I", str(tmp), "-I", str(ROOT / "tests")]
            obj, executable = tmp / "probe.o", tmp / "probe-test"
            subprocess.run(flags + ["-Dmain=net_probe_main", "-c",
                           str(ROOT / "net_probe/main.c"), "-o", str(obj)], check=True)
            subprocess.run(flags + [str(ROOT / "tests/net_probe_test.c"), str(obj),
                           "-o", str(executable)], check=True)
            for scenario in range(13):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(executable), str(scenario)], check=True, timeout=3)
