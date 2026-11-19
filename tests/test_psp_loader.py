"""Regression: CIRCLE/HOME and load failure exit through the user launcher."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LoaderExitTest(unittest.TestCase):
    def test_exit_ownership(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            shim = (ROOT / "tests" / "psp_loader_shim.h").read_text()
            for name in ("pspkernel.h", "pspdebug.h", "pspctrl.h",
                         "psputility_usbmodules.h", "kubridge.h"):
                (tmp / name).write_text(shim)
            flags = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(tmp)]
            obj = tmp / "loader.o"
            executable = tmp / "loader-test"
            subprocess.run(flags + ["-Dmain=psp_loader_main", "-c",
                           str(ROOT / "experiments/psp_link/app/main.c"), "-o", str(obj)], check=True)
            subprocess.run(flags + [str(ROOT / "tests/psp_loader_test.c"), str(obj),
                           "-o", str(executable)], check=True)
            for scenario in ("0", "1", "2"):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(executable), scenario], check=True, timeout=3)
