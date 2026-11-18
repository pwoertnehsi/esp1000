"""Actual virtual-interface launcher ownership and failure cleanup."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VirtualNetLoaderTest(unittest.TestCase):
    def test_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            for name in ("pspkernel.h", "pspdebug.h", "pspctrl.h", "psputility_netmodules.h",
                         "pspnet.h", "pspnet_inet.h", "kubridge.h", "psputility_usbmodules.h"):
                (tmp / name).write_text('#include "vnet_loader_shim.h"\n')
            # Static result storage must fit the PSP's 32-bit argument address.
            flags = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fno-pie",
                     "-Wno-pointer-to-int-cast", "-I", str(tmp), "-I", str(ROOT / "tests")]
            obj, executable = tmp / "loader.o", tmp / "loader-test"
            subprocess.run(flags + ["-Dmain=vnet_loader_main", "-c",
                           str(ROOT / "virtual_net/app/main.c"), "-o", str(obj)], check=True)
            subprocess.run(flags + ["-no-pie", str(ROOT / "tests/vnet_loader_test.c"),
                           str(obj), "-o", str(executable)], check=True)
            for scenario in range(23):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(executable), str(scenario)], check=True, timeout=3)

            # Build/run the conditional USB launcher too; same ownership gates.
            usb_flags = flags + ["-DVNET_USB_LAB"]
            subprocess.run(usb_flags + ["-Dmain=vnet_loader_main", "-c",
                           str(ROOT / "virtual_net/app/main.c"), "-o", str(obj)], check=True)
            subprocess.run(usb_flags + ["-no-pie", str(ROOT / "tests/vnet_loader_test.c"),
                           str(obj), "-o", str(executable)], check=True)
            for scenario in range(23):
                with self.subTest(backend="usb", scenario=scenario):
                    subprocess.run([str(executable), str(scenario)], check=True, timeout=3)

            subprocess.run([str(executable), "16"], check=True, timeout=3,
                           env={**os.environ, "USB_GATE_TEST": "1"})

            subprocess.run([str(executable), "16"], check=True, timeout=3,
                           env={**os.environ, "HTTP_GATE_TEST": "1"})
            subprocess.run([str(executable), "19"], check=True, timeout=3,
                           env={**os.environ, "HTTP_CANCEL_TEST": "1"})
