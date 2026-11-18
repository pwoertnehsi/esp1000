"""Exercise real native-interface lab client with mocked PSP APIs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VirtualNetLabClientTest(unittest.TestCase):
    def test_results_and_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp / 'pspnet_inet.h').write_text('#include "native_socket_shim.h"\n')
            executable = tmp / 'client-test'
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I', str(tmp), '-I', str(ROOT / 'tests'), '-I', str(ROOT / 'common'),
                            str(ROOT / 'tests/vnet_lab_client_test.c'),
                            str(ROOT / 'virtual_net/app/lab_client.c'),
                            str(ROOT / 'common/lab_gateway.c'), '-o', str(executable)], check=True)
            for scenario in range(17):
                with self.subTest(scenario=scenario):
                    subprocess.run([str(executable), str(scenario)], check=True, timeout=3)
