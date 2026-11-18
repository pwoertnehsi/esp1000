"""Test the real serial capture process against a pseudo-terminal peer."""
import os
from pathlib import Path
import pty
import select
import subprocess
import sys
import tempfile
import time
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "esp_uart.py"


class UartTest(unittest.TestCase):
    def test_commands_and_capture(self):
        master, slave = pty.openpty()
        try:
            with tempfile.TemporaryDirectory() as tmp:
                log = Path(tmp) / "uart.log"
                process = subprocess.Popen(
                    [sys.executable, str(SCRIPT), "--port", os.ttyname(slave),
                     "--seconds", "1.5", "--command", "status", "--log", str(log)],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    received = b""
                    deadline = time.monotonic() + 3
                    while b"status\n" not in received and time.monotonic() < deadline:
                        if select.select([master], [], [], 0.1)[0]:
                            received += os.read(master, 1024)
                    self.assertIn(b"status\n", received)
                    reply = b"STATUS mounted=0 TX=0 RX=0\r\n"
                    os.write(master, reply)
                    stdout, stderr = process.communicate(timeout=4)
                    self.assertEqual(process.returncode, 0, stderr)
                    self.assertIn(reply, stdout)
                    self.assertEqual(log.read_bytes(), reply)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.communicate()
        finally:
            os.close(master)
            os.close(slave)


if __name__ == "__main__":
    unittest.main()
