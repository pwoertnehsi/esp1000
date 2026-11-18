"""Actual add-only configuration: ABI layout, per-call logs, cancellation and cleanup."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VirtualNetConfigTest(unittest.TestCase):
    def test_configure(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp / 'pspnet_inet.h').write_text('#include "native_socket_shim.h"\n')
            executable = tmp / 'config-test'
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I', str(tmp), '-I', str(ROOT / 'tests'), '-I', str(ROOT / 'common'),
                            str(ROOT / 'tests/vnet_config_test.c'),
                            str(ROOT / 'virtual_net/app/native_config.c'),
                            str(ROOT / 'common/lab_gateway.c'), '-o', str(executable)], check=True)
            for scenario in range(8):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(executable), str(scenario)], check=True, timeout=3)
