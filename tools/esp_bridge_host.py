#!/usr/bin/env python3
"""Run ON THE HOST: a single-port UART/ESP flash helper, without a shell or socket."""
import argparse
from contextlib import ExitStack
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import select
import stat
import subprocess
import sys
import tempfile
import termios
import time

ROOT = Path(__file__).resolve().parents[1]
REQUEST_NAME = re.compile(r"([0-9a-f]{32})\.request\.json\Z")
MAX_OUTPUT = 262144
FLASH_FILES = (
    ("0x0", "bootloader/bootloader.bin", 0x8000),
    ("0x8000", "partition_table/partition-table.bin", 0x1000),
    ("0x10000", "psp_link_host.bin", 0x100000),
)


def open_mailbox(path):
    path.mkdir(mode=0o700, exist_ok=True)
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    info = os.fstat(fd)
    if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) != 0o700:
        os.close(fd)
        raise ValueError("Bridge directory must belong to you and have mode 0700")
    return fd


def write_json(directory, name, value):
    temporary = name + ".tmp"
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                 0o600, dir_fd=directory)
    try:
        with os.fdopen(fd, "w") as stream:
            json.dump(value, stream)
        os.rename(temporary, name, src_dir_fd=directory, dst_dir_fd=directory)
    finally:
        try:
            os.unlink(temporary, dir_fd=directory)
        except FileNotFoundError:
            pass


def read_json(directory, name):
    fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > 16384:
            raise ValueError("Request must be a regular JSON file, at most 16 KiB")
        return json.loads(stream.read(16385))


def validate_request(request):
    if not isinstance(request, dict):
        raise ValueError("Request must be an object")
    op = request.get("op")
    if op in ("status", "flash"):
        if set(request) != {"op"}:
            raise ValueError("This operation takes no arguments")
        return request
    if op != "capture" or set(request) - {"op", "seconds", "commands"}:
        raise ValueError("Allowed operations: status, capture, flash")
    seconds = request.get("seconds", 15)
    if (isinstance(seconds, bool) or not isinstance(seconds, (int, float)) or
            not math.isfinite(seconds) or not 1 <= seconds <= 60):
        raise ValueError("Capture duration must be 1..60 seconds")
    commands = request.get("commands", [])
    if not isinstance(commands, list) or len(commands) > 16:
        raise ValueError("At most 16 UART commands")
    for command in commands:
        if (not isinstance(command, str) or not command or len(command) > 80 or
                any(ord(ch) < 32 or ord(ch) > 126 for ch in command)):
            raise ValueError("Commands must be printable ASCII, 1..80 characters")
        if (command not in {"help", "status", "ping", "tlscheck", "reset", "auto on", "auto off",
                            "vbus on", "vbus off"} and
                not command.startswith(("ping ", "say "))):
            raise ValueError("Command is outside the PSP link firmware allowlist")
    return {"op": op, "seconds": seconds, "commands": commands}


def capture(port, seconds, commands):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        if not stat.S_ISCHR(os.fstat(fd).st_mode):
            raise ValueError("Selected serial port is not a character device")
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        settings = termios.tcgetattr(fd)
        settings[0] = settings[1] = settings[3] = 0
        settings[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        settings[4] = settings[5] = termios.B115200
        settings[6][termios.VMIN] = settings[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, settings)
        reader = select.poll()
        reader.register(fd, select.POLLIN | select.POLLHUP | select.POLLERR)
        pending = [command.encode("ascii") + b"\n" for command in commands]
        started = time.monotonic()
        next_command = started + 0.3
        output = bytearray()
        truncated = False
        while time.monotonic() - started < seconds:
            now = time.monotonic()
            if pending and now >= next_command and select.select([], [fd], [], 0)[1]:
                written = os.write(fd, pending[0])
                pending[0] = pending[0][written:]
                if not pending[0]:
                    pending.pop(0)
                    next_command = now + 0.25
            for _, flags in reader.poll(50):
                if flags & (select.POLLHUP | select.POLLERR | select.POLLNVAL):
                    raise OSError(f"Serial device disconnected (poll flags=0x{flags:x})")
                if not flags & select.POLLIN:
                    continue
                try:
                    data = os.read(fd, 4096)
                except BlockingIOError:
                    continue
                # VMIN=VTIME=0 permits an empty read without a hangup. Poll's
                # explicit HUP/ERR bits, not a zero-length read, detect loss.
                if not data:
                    continue
                remaining = MAX_OUTPUT - len(output)
                output.extend(data[:remaining])
                truncated |= len(data) > remaining
        return {"output": output.decode("utf-8", errors="replace"),
                "truncated": truncated, "unsent_commands": len(pending)}
    finally:
        os.close(fd)


def flash(port, root=ROOT):
    # Never execute project scripts or consume flash_args from the workspace.
    # Snapshot only the three fixed binary inputs; no requested paths/offsets.
    images = []
    for offset, name, limit in FLASH_FILES:
        # Walk directory FDs without following symlinks, including parents.
        with ExitStack() as cleanup:
            directory = os.open(root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            cleanup.callback(os.close, directory)
            parts = ("esp_link", "build", *Path(name).parts)
            for part in parts[:-1]:
                directory = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                                    dir_fd=directory)
                cleanup.callback(os.close, directory)
            fd = os.open(parts[-1], os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                         dir_fd=directory)
            with os.fdopen(fd, "rb") as stream:
                if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                    raise ValueError("Firmware input must be a regular file")
                data = stream.read(limit + 1)
        if not data or len(data) > limit:
            raise ValueError(f"Invalid firmware size: {name}")
        images.append((offset, name, data))
    with tempfile.TemporaryDirectory(prefix="psp-esp-flash-") as temporary:
        args = [sys.executable, "-I", "-m", "esptool", "--chip", "esp32s3",
                "--port", port, "--baud", "460800", "--before", "default-reset",
                "--after", "hard-reset", "write-flash", "--flash-mode", "dio",
                "--flash-freq", "80m", "--flash-size", "2MB"]
        hashes = {}
        for index, (offset, name, data) in enumerate(images):
            target = Path(temporary) / f"image-{index}.bin"
            target.write_bytes(data)
            args.extend((offset, str(target)))
            hashes[name] = hashlib.sha256(data).hexdigest()
        # -I prevents workspace modules / PYTHONPATH from being loaded by Python.
        # Installed esptool and its dependencies must belong to the host venv.
        result = subprocess.run(args, cwd=temporary, capture_output=True,
                                timeout=120, check=False)
        output = result.stdout + result.stderr
        return {"exit_code": result.returncode, "sha256": hashes,
                "output": output[-MAX_OUTPUT:].decode("utf-8", errors="replace"),
                "truncated": len(output) > MAX_OUTPUT}


def serve(port, mailbox):
    directory = open_mailbox(mailbox)
    lock = os.open("host.lock", os.O_WRONLY | os.O_CREAT | os.O_NOFOLLOW | os.O_NONBLOCK,
                   0o600, dir_fd=directory)
    try:
        if not stat.S_ISREG(os.fstat(lock).st_mode):
            raise ValueError("Bridge lock must be a regular file")
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        print(f"Bridge ready: {port}\nMailbox: {mailbox}\nCtrl+C stops access.", flush=True)
        last_status = 0
        while True:
            if time.monotonic() - last_status >= 1:
                write_json(directory, "host.json", {"port": port, "pid": os.getpid(),
                           "updated": time.time(), "present": os.path.exists(port)})
                last_status = time.monotonic()
            for name in sorted(os.listdir(directory)):
                match = REQUEST_NAME.fullmatch(name)
                if not match:
                    continue
                reply = {"ok": False}
                try:
                    request = validate_request(read_json(directory, name))
                    os.unlink(name, dir_fd=directory)
                    op = request["op"]
                    print(f"{time.strftime('%H:%M:%S')} {op}", flush=True)
                    if op == "status":
                        result = {"port": port, "present": os.path.exists(port)}
                    elif op == "capture":
                        result = capture(port, request["seconds"], request["commands"])
                    else:
                        result = flash(port)
                    reply = {"ok": result.get("exit_code", 0) == 0, "result": result}
                except (OSError, ValueError, subprocess.TimeoutExpired) as error:
                    reply = {"ok": False, "error": str(error)}
                    try:
                        os.unlink(name, dir_fd=directory)
                    except FileNotFoundError:
                        pass
                write_json(directory, match[1] + ".reply.json", reply)
            time.sleep(0.1)
    finally:
        os.close(lock)
        os.close(directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyUSB0",
                        help="Chosen once at startup; requests cannot change it")
    args = parser.parse_args()
    if not os.path.isabs(args.port):
        parser.error("--port must be an absolute device path")
    try:
        serve(args.port, ROOT / ".uart-bridge")
    except KeyboardInterrupt:
        print("\nBridge stopped.")
    except (OSError, ValueError) as error:
        parser.exit(1, f"Bridge failed: {error}\n")


if __name__ == "__main__":
    main()
