#!/usr/bin/env python3
"""Task-side client for the explicitly started, single-port host helper."""
import argparse
import json
import os
from pathlib import Path
import sys
import time
import uuid

MAILBOX = Path(__file__).resolve().parents[1] / ".uart-bridge"


def request(value, timeout):
    directory = os.open(MAILBOX, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    identity = uuid.uuid4().hex
    temporary = identity + ".tmp"
    name = identity + ".request.json"
    response = identity + ".reply.json"
    try:
        fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                     0o600, dir_fd=directory)
        with os.fdopen(fd, "w") as stream:
            json.dump(value, stream)
        os.rename(temporary, name, src_dir_fd=directory, dst_dir_fd=directory)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                fd = os.open(response, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory)
            except FileNotFoundError:
                time.sleep(0.1)
                continue
            with os.fdopen(fd) as stream:
                result = json.load(stream)
            os.unlink(response, dir_fd=directory)
            return result
        raise TimeoutError("No bridge reply; ensure the host helper is running. "
                           "An operation already started may still finish; do not blindly retry flash.")
    finally:
        # Remove unclaimed work on timeout, so it cannot run much later.
        for pending in (temporary, name):
            try:
                os.unlink(pending, dir_fd=directory)
            except FileNotFoundError:
                pass
        os.close(directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="op", required=True)
    sub.add_parser("status")
    sub.add_parser("flash")
    capture = sub.add_parser("capture")
    capture.add_argument("--seconds", type=float, default=15)
    capture.add_argument("--command", action="append", default=[])
    args = parser.parse_args()
    value = {"op": args.op}
    if args.op == "capture":
        value.update(seconds=args.seconds, commands=args.command)
    try:
        reply = request(value, 190)
        result = reply.get("result", {})
        output = result.pop("output", None)
        if output:
            print(output, end="" if output.endswith("\n") else "\n")
        print(json.dumps({**reply, "result": result}, indent=2))
        return 0 if reply.get("ok") else 1
    except (OSError, ValueError, TimeoutError) as error:
        print(f"Bridge client: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
