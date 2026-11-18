"""Compile the actual kernel helper against a module/hook model."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class BrowserHelperTest(unittest.TestCase):
    def test_ownership_and_guards(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for header in ("pspkernel.h","psploadcore.h","pspsdk.h","systemctrl.h"):
                (tmp/header).write_text('#include "browser_helper_shim.h"\n')
            exe=tmp/"browser-helper-test"
            subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-Wno-pointer-to-int-cast","-Wno-int-to-pointer-cast",
                            "-fno-pie","-no-pie","-fsanitize=address,undefined","-I",str(tmp),"-I",str(ROOT/"tests"),
                            str(ROOT/"tests/browser_helper_test.c"),str(ROOT/"browser_local/driver/path_patch.c"),str(ROOT/"browser_local/driver/code_patch.c"),"-o",str(exe)],check=True)
            for scenario in range(18):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(exe),str(scenario)],check=True,timeout=3,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})
