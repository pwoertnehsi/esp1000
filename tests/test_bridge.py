import importlib.util
import os
from pathlib import Path
import pty
import select
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


host = load("esp_bridge_host")
client = load("esp_bridge")


class BridgeTest(unittest.TestCase):
    def test_request_limits(self):
        self.assertEqual(host.validate_request({"op": "status"}), {"op": "status"})
        self.assertEqual(host.validate_request({"op": "capture"})["seconds"], 15)
        self.assertEqual(host.validate_request({"op":"capture","commands":["tlscheck"]})["commands"],["tlscheck"])
        for value in [
            {"op": "exec", "command": "id"},
            {"op": "flash", "port": "/dev/sda"},
            {"op": "flash", "path": "/tmp/image"},
            {"op": "capture", "seconds": float("nan")},
            {"op": "capture", "seconds": True},
            {"op": "capture", "seconds": 61},
            {"op": "capture", "commands": ["status\nreset"]},
            {"op": "capture", "commands": ["shell id"]},
            {"op": "capture", "commands": ["say " + "x" * 80]},
            {"op": "capture", "commands": ["status"] * 17},
        ]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                host.validate_request(value)

    def test_mailbox_and_symlink_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = host.open_mailbox(Path(tmp))
            try:
                host.write_json(directory, "test.json", {"op": "status"})
                self.assertEqual(host.read_json(directory, "test.json"), {"op": "status"})
                (Path(tmp) / "link.json").symlink_to(Path(tmp) / "test.json")
                with self.assertRaises(OSError):
                    host.read_json(directory, "link.json")
            finally:
                os.close(directory)

    def test_uart_with_pseudo_terminal(self):
        master, slave = pty.openpty()
        received = []

        def peer():
            if select.select([master], [], [], 2)[0]:
                received.append(os.read(master, 4096))
                os.write(master, b"STATUS mounted=0\r\n")

        thread = threading.Thread(target=peer)
        thread.start()
        try:
            result = host.capture(os.ttyname(slave), 1, ["status"])
            thread.join(timeout=3)
            self.assertIn(b"status\n", b"".join(received))
            self.assertEqual(result["output"], "STATUS mounted=0\r\n")
            self.assertEqual(result["unsent_commands"], 0)
        finally:
            thread.join(timeout=3)
            os.close(master)
            os.close(slave)

    def test_empty_read_and_eagain_are_not_disconnects(self):
        master, slave = pty.openpty()
        read = os.read
        calls = 0

        def transient_read(fd, count):
            nonlocal calls
            calls += 1
            if calls == 1:
                return b""
            if calls == 2:
                raise BlockingIOError()
            return read(fd, count)

        try:
            os.write(master, b"still connected\n")
            with patch.object(host.os, "read", side_effect=transient_read):
                result = host.capture(os.ttyname(slave), 0.2, [])
            self.assertIn("still connected", result["output"])
            self.assertGreaterEqual(calls, 3)
        finally:
            os.close(master)
            os.close(slave)

    def test_explicit_serial_hangup_is_reported(self):
        master, slave = pty.openpty()
        try:
            with patch.object(host.select, "poll") as poll:
                poll.return_value.poll.return_value = [(slave, select.POLLHUP)]
                with self.assertRaisesRegex(OSError, "poll flags=0x10"):
                    host.capture(os.ttyname(slave), 0.2, [])
        finally:
            os.close(master)
            os.close(slave)

    def test_fixed_flash_and_symlink_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            build = root / "esp_link" / "build"
            for _, name, _ in host.FLASH_FILES:
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"test firmware")
            (build / "flash_args").write_text("malicious command line ignored")
            snapshots = []

            def fake_run(args, **kwargs):
                self.assertEqual(args[:4], [sys.executable, "-I", "-m", "esptool"])
                self.assertEqual(args[args.index("--port") + 1], "/dev/ttyUSB0")
                self.assertNotIn("shell", kwargs)
                self.assertEqual(args[-6::2], ["0x0", "0x8000", "0x10000"])
                snapshots.extend(Path(name).read_bytes() for name in args[-5::2])
                return subprocess.CompletedProcess(args, 0, b"verified\n", b"")

            with patch.object(host.subprocess, "run", side_effect=fake_run) as run:
                result = host.flash("/dev/ttyUSB0", root)
                self.assertEqual(result["exit_code"], 0)
                self.assertEqual(snapshots, [b"test firmware"] * 3)
                image = build / "psp_link_host.bin"
                image.unlink()
                image.symlink_to(build / "bootloader" / "bootloader.bin")
                with self.assertRaises(OSError):
                    host.flash("/dev/ttyUSB0", root)
                self.assertEqual(run.call_count, 1)

    def test_host_client_exchange(self):
        with tempfile.TemporaryDirectory() as tmp:
            script = str(ROOT / "tools" / "esp_bridge_host.py")
            code = ("import runpy,sys; from pathlib import Path; "
                    "runpy.run_path(sys.argv[1])['serve']('/dev/not-a-real-esp', Path(sys.argv[2]))")
            process = subprocess.Popen([sys.executable, "-I", "-c", code, script, tmp],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                self.assertTrue(select.select([process.stdout], [], [], 3)[0])
                self.assertIn(b"Bridge ready:", process.stdout.readline())
                with patch.object(client, "MAILBOX", Path(tmp)):
                    reply = client.request({"op": "status"}, 3)
                    self.assertTrue(reply["ok"])
                    self.assertFalse(reply["result"]["present"])
                    reply = client.request({"op": "flash", "port": "/dev/sda"}, 3)
                    self.assertFalse(reply["ok"])
                self.assertFalse(list(Path(tmp).glob("*.request.json")))
            finally:
                process.terminate()
                process.communicate(timeout=3)


if __name__ == "__main__":
    unittest.main()
