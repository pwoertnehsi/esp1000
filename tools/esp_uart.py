#!/usr/bin/env python3
"""Bounded UART command/log capture; Linux, standard library only."""
import argparse
import os
from pathlib import Path
import select
import sys
import termios
import time


def configure(fd):
    settings = termios.tcgetattr(fd)
    settings[0] = 0
    settings[1] = 0
    settings[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    settings[3] = 0
    settings[4] = settings[5] = termios.B115200
    settings[6][termios.VMIN] = 0
    settings[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, settings)


def capture(port, seconds, commands, log=None, startup_wait=0.5):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    output = None
    try:
        configure(fd)
        if log:
            Path(log).parent.mkdir(parents=True, exist_ok=True)
            output = open(log, "xb")  # Preserve earlier test evidence.
        start = time.monotonic()
        pending = [line.encode("ascii") + b"\n" for line in commands]
        next_command = start + startup_wait
        while time.monotonic() - start < seconds:
            now = time.monotonic()
            if pending and now >= next_command:
                data = pending[0]
                if select.select([], [fd], [], 0)[1]:
                    sent = os.write(fd, data)
                    if sent == len(data):
                        pending.pop(0)
                        next_command = now + 0.25
                    else:
                        pending[0] = data[sent:]
            if select.select([fd], [], [], min(0.05, max(0, seconds - (now - start))))[0]:
                data = os.read(fd, 4096)
                if data:
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
                    if output:
                        output.write(data)
                        output.flush()
        if pending:
            raise TimeoutError("Capture ended before all UART commands were sent")
    finally:
        if output:
            output.close()
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--seconds", type=float, default=15)
    parser.add_argument("--command", action="append", default=[])
    parser.add_argument("--log")
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    try:
        capture(args.port, args.seconds, args.command, args.log)
    except (OSError, ValueError, TimeoutError) as error:
        parser.exit(1, f"UART capture failed: {error}\n")


if __name__ == "__main__":
    main()
