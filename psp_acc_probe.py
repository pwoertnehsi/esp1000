#!/usr/bin/env python3
"""
psp_acc_probe.py (v2) - play the PSP camera's USB *host* role from a Linux PC.

The PSP is the USB device (054c:0282, "USB Accessory"); the camera is the host.
This replays the camera's control requests (from the PulseView captures).

Setup:  PC -> breakout (14.7k from ID to GND on the PSP-side plug) -> PSP
Before running (no USB audio device on this PC, so this is safe):
    sudo modprobe -r snd_usb_audio        # stop Linux grabbing the audio interfaces
Start this script FIRST, then open the camera app on the PSP.
Also run   sudo dmesg -w   in another terminal and keep the output.

v2 changes: retries the first request for a window instead of giving up at once,
notices when the PSP re-enumerates (new device address), classifies errors, and
no longer detaches kernel drivers before the handshake.

Requires: python-pyusb, libusb   (Arch: pacman -S python-pyusb libusb)
"""
import argparse
import errno
import sys
import time

import usb.core

VID, PID = 0x054C, 0x0282
T0 = None


def stamp():
    return "%7.3f s" % (time.time() - T0)


def hexs(b):
    return " ".join("%02x" % x for x in b)


def why(e):
    n = getattr(e, "errno", None)
    names = {errno.EPIPE: "STALL (device refused the request)",
             errno.ETIMEDOUT: "timeout (no answer)",
             errno.EIO: "I/O / protocol error (device gone, reset, or not answering)",
             errno.ENODEV: "device disappeared",
             errno.EBUSY: "interface busy (a kernel driver has it)"}
    return "%s [errno %s]" % (names.get(n, str(e)), n)


def find():
    return usb.core.find(idVendor=VID, idProduct=PID)


def ctrl_in(bm, req, val, idx, length, label):
    dev = find()
    if dev is None:
        raise usb.core.USBError("device gone", errno=errno.ENODEV)
    data = bytes(dev.ctrl_transfer(bm, req, val, idx, length, timeout=200))
    print("%s  %-34s -> %s" % (stamp(), label, hexs(data) or "(empty)"))
    return data


def ctrl_out(bm, req, val, idx, payload, label):
    dev = find()
    if dev is None:
        raise usb.core.USBError("device gone", errno=errno.ENODEV)
    n = dev.ctrl_transfer(bm, req, val, idx, payload, timeout=200)
    print("%s  %-34s sent %d bytes: %s" % (stamp(), label, n, hexs(payload) or "-"))


def first_request(window):
    """Keep trying C1 01 until it works or the window closes."""
    end = time.time() + window
    last_addr, last_err, fails = None, None, 0
    while time.time() < end:
        dev = find()
        if dev is None:
            time.sleep(0.002)
            continue
        if last_addr is not None and dev.address != last_addr:
            print("%s  PSP re-enumerated: address %d -> %d" % (stamp(), last_addr, dev.address))
        if last_addr is None:
            print("%s  device present (bus %d addr %d)" % (stamp(), dev.bus, dev.address))
        last_addr = dev.address
        try:
            return ctrl_in(0xC1, 0x01, 0x0003, 0x0000, 1, "C1 01 wValue=0003 (type match)")
        except usb.core.USBError as e:
            fails += 1
            msg = why(e)
            if msg != last_err or fails <= 3:
                print("%s  attempt %d failed: %s" % (stamp(), fails, msg))
                last_err = msg
            time.sleep(0.005)
    print("%s  gave up after %d failed attempts" % (stamp(), fails))
    return None


def rest_of_handshake():
    ctrl_out(0x41, 0x01, 0x0000, 0x0000, bytes([0, 0, 1, 0, 1, 0x10, 0x27, 3]),
             "41 01 accessory info (8 bytes)")
    ctrl_in(0xC1, 0x03, 0x0000, 0x0003, 5, "C1 03 wIndex=3")
    ctrl_in(0xC1, 0x08, 0x0000, 0x0003, 8, "C1 08 wIndex=3")


def stage2():
    print("%s  --- stage 2: app-streaming requests ---" % stamp())
    ctrl_out(0x41, 0x09, 0x002B, 0x0003, bytes(4), "41 09 wValue=002B")
    ctrl_out(0x01, 0x0B, 1, 2, b"", "SET_INTERFACE(2, alt 1)")
    ctrl_out(0x41, 0x09, 0x0009, 0x0003, bytes([0x27, 0x03]), "41 09 wValue=0009")
    ctrl_out(0x01, 0x0B, 1, 3, b"", "SET_INTERFACE(3, alt 1)")


def poll_ep1(seconds):
    print("%s  --- polling EP1 (interrupt IN) for %.1f s ---" % (stamp(), seconds))
    end, got = time.time() + seconds, 0
    while time.time() < end:
        dev = find()
        if dev is None:
            print("%s  device gone" % stamp())
            break
        try:
            d = bytes(dev.read(0x81, 64, timeout=100))
            got += 1
            print("%s  EP1: %s" % (stamp(), hexs(d)))
        except usb.core.USBTimeoutError:
            pass
        except usb.core.USBError as e:
            print("%s  EP1: %s" % (stamp(), why(e)))
            break
    print("%s  EP1 packets: %d" % (stamp(), got))


def main():
    global T0
    ap = argparse.ArgumentParser()
    ap.add_argument("--wait", type=float, default=60.0, help="seconds to wait for the PSP to appear")
    ap.add_argument("--window", type=float, default=2.5, help="seconds to keep retrying the first request")
    ap.add_argument("--delay", type=float, default=0.0, help="seconds to wait after the device appears")
    ap.add_argument("--full", action="store_true", help="also send stage-2 requests")
    ap.add_argument("--poll", type=float, default=5.0)
    args = ap.parse_args()

    print("Waiting for 054c:0282 ... open the camera app on the PSP now.")
    end = time.time() + args.wait
    while find() is None:
        if time.time() > end:
            print("PSP did not appear.")
            return 1
        time.sleep(0.002)
    T0 = time.time()
    if args.delay:
        time.sleep(args.delay)

    if first_request(args.window) is None:
        print("Handshake did not start. Send me the dmesg lines from this moment.")
        return 2
    try:
        rest_of_handshake()
        if args.full:
            stage2()
    except usb.core.USBError as e:
        print("%s  handshake stopped: %s" % (stamp(), why(e)))
    poll_ep1(args.poll)
    print("%s  done. Did the 'connect a compatible camera' message go away?" % stamp())
    return 0


if __name__ == "__main__":
    sys.exit(main())
