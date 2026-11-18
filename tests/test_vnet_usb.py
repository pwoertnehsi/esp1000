"""Compile actual USB transport with SDK declarations and mocked device IO."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class UsbTransportTest(unittest.TestCase):
    def test_transport(self):
        config = shutil.which("psp-config")
        if not config:
            self.skipTest("PSPSDK declarations unavailable")
        sdk = Path(subprocess.check_output([config, "--pspsdk-path"], text=True).strip()) / "include"
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp / "pspkernel.h").write_text("""#include <stdint.h>
typedef uint32_t u32;
typedef uint16_t u16;
u32 sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned);
void sceKernelDcacheWritebackRange(const void *,unsigned);
void sceKernelDcacheInvalidateRange(const void *,unsigned);
""")
            (tmp / "pspsdk.h").write_text("")
            # Copy only the standalone USB declarations, avoiding SDK libc headers.
            for header in ("pspusb.h", "pspusbbus.h"):
                shutil.copyfile(sdk / header, tmp / header)
            exe = tmp / "usb-test"
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", "-I", str(tmp), "-I", str(ROOT / "common"),
                            str(ROOT / "tests/vnet_usb_test.c"), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=5,
                           env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
