#!/usr/bin/env python3
"""
patch_enum.py - make ESP-IDF's USB host enumeration PSP-friendly.

Why: the stock host library finishes enumeration with SET_CONFIGURATION. The
PSP appears to end the session if the accessory handshake does not come before
that. This patch changes the LAST enumeration stage so that, instead of
SET_CONFIGURATION, it sends the camera's first request (C1 01, wValue=3,
wLength=1). The application then sends the remaining requests itself and
SET_CONFIGURATION last, which the host library allows through normal control
transfers.

It also logs the reply of that request from the enumeration driver.

Usage:
    python3 tools/patch_enum.py                # auto-find enum.c
    python3 tools/patch_enum.py /path/to/enum.c
    python3 tools/patch_enum.py --revert [/path/to/enum.c]

It makes enum.c.orig next to the file, refuses to patch anything it does not
recognise, and is safe to run twice.
"""
import glob
import os
import re
import shutil
import sys

MARK = "PSP-PATCH"

SETCFG = re.compile(
    r"(?P<indent>[ \t]*)USB_SETUP_PACKET_INIT_SET_CONFIG\(\s*\(usb_setup_packet_t\s*\*\)\s*"
    r"transfer->data_buffer\s*,\s*bConfigurationValue\s*\)\s*;[^\n]*\n"
    r"(?P<body>(?:[^\n]*\n){0,3}?[^\n]*expect_num_bytes\s*=\s*0\s*;[^\n]*\n)")

CHECKCFG = re.compile(
    r"static\s+esp_err_t\s+check_config\s*\(\s*void\s*\)\s*\{[^}]*?return\s+ESP_OK\s*;\s*\}", re.S)

NEW_REQUEST = """{i}/* {m}: send the accessory handshake instead of SET_CONFIGURATION */
{i}{{
{i}    usb_setup_packet_t *psp_setup = (usb_setup_packet_t *)transfer->data_buffer;
{i}    psp_setup->bmRequestType = 0xC1;
{i}    psp_setup->bRequest = 0x01;
{i}    psp_setup->wValue = 0x0003;
{i}    psp_setup->wIndex = 0x0000;
{i}    psp_setup->wLength = 1;
{i}    transfer->num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(1, ctrl_ep_mps);
{i}    p_enum_driver->single_thread.expect_num_bytes = sizeof(usb_setup_packet_t) + 1;
{i}}}
"""

NEW_CHECK = """static esp_err_t check_config(void)
{
    /* PSP-PATCH: report the reply to the C1 01 handshake sent in this stage */
    usb_transfer_t *psp_x = &p_enum_driver->constant.urb->transfer;
    int psp_n = psp_x->actual_num_bytes - (int)sizeof(usb_setup_packet_t);
    ESP_LOGW(ENUM_TAG, "PSP-HS: C1 01 reply %d byte(s), first = 0x%02x", psp_n,
             psp_n > 0 ? psp_x->data_buffer[sizeof(usb_setup_packet_t)] : 0xff);
    return ESP_OK;
}"""


def candidates():
    here = os.getcwd()
    paths = glob.glob(os.path.join(here, "managed_components", "*usb*", "enum.c"))
    paths += glob.glob(os.path.join(here, "managed_components", "*usb*", "**", "enum.c"), recursive=True)
    idf = os.environ.get("IDF_PATH")
    if idf:
        paths.append(os.path.join(idf, "components", "usb", "enum.c"))
    seen, out = set(), []
    for p in paths:
        if os.path.isfile(p) and p not in seen:
            seen.add(p)
            out.append(p)
    return out


def patch(path):
    src = open(path).read()
    if MARK in src:
        print("already patched:", path)
        return 0
    m = SETCFG.search(src)
    if not m or (m.end() - m.start()) > 700:
        print("ERROR: could not find the SET_CONFIGURATION request code in", path)
        print("The file differs from what this patch expects. Look for")
        print("  USB_SETUP_PACKET_INIT_SET_CONFIG(...)  inside case ENUM_STAGE_SET_CONFIG")
        print("and replace that case body with the C1 01 request shown in this script.")
        return 2
    shutil.copy2(path, path + ".orig")
    out = src[:m.start()] + NEW_REQUEST.format(i=m.group("indent"), m=MARK) + src[m.end():]
    c = CHECKCFG.search(out)
    if c:
        out = out[:c.start()] + NEW_CHECK + out[c.end():]
    else:
        print("note: check_config() not found; the reply will not be logged by the enum driver")
    open(path, "w").write(out)
    print("patched:", path)
    print("backup :", path + ".orig")
    return 0


def revert(path):
    if os.path.isfile(path + ".orig"):
        shutil.copy2(path + ".orig", path)
        print("restored", path)
        return 0
    print("no backup found for", path)
    return 1


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    do_revert = "--revert" in sys.argv
    paths = args or candidates()
    if not paths:
        print("enum.c not found. Pass its path, e.g. managed_components/espressif__usb/enum.c")
        return 1
    rc = 0
    for p in paths:
        print("--", p)
        rc |= revert(p) if do_revert else patch(p)
    return rc


if __name__ == "__main__":
    sys.exit(main())
