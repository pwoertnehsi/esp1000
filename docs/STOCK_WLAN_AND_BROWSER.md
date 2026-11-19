# Stock WLAN and Browser Integration

## Installation and User Experience

Supported integration target: **6.61 ARK-5 on PSP-E1000**. Other CFWs are
out of scope for now. The ESP backend must work without ARK's native-radio WPA2
extension: the user reports that it is omitted on E1000. No CFW reinstall or
flash replacement is needed for the ESP's radio security.

Target a memory-stick CFW plugin with reversible RAM hooks, clear adapter status,
and a documented disable path. Preserve the stock network setup dialog and saved
profiles wherever their interfaces permit it. Avoid flash0 replacement and global
model spoofing. Keep credentials out of logs and replace the bench-only compiled
credentials with runtime configuration before distributing a user build.

The next test is the separate [compatibility probe](../net_probe/README.txt).
Build/package with `make probe`; install `build/net-probe.zip` by copying its
`PSP` directory onto the memory stick. It does not replace the working USB app.
The initial inventory is read-only apart from its memory-stick log. CROSS opts
into stock COMMON/INET loading, whose own startup code may touch absent hardware.

It logs firmware/model/CFW version numbers, selected loaded-module metadata,
import/export table addresses and sizes, and PRX filenames under `flash0:/kd`
and `flash0:/vsh/module`. It does **not** dump library NIDs, validate private
function prototypes, or establish that an interface can attach. Directory lookup
failures must be distinguished from actual absent files. It records and syncs
before each load/unload, and releases only modules acquired by that run.

## WPA2: Radio Security and PSP Compatibility Are Different

The current ESP station backend already has WPA2-Personal support in its code
path. ESP-IDF performs association and key exchange. Native scan rows explicitly
recognize WPA2 and map AES networks to Sony's existing WPA-PSK (AES) profile
format. The saved profile supplies the passphrase (8–63 characters) or raw PSK;
profile security value4 is Sony's AES format, not a requirement for WPA1.
The ESP auth threshold is a minimum, not an exact protocol selection (see the
[ESP-IDF guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/wifi-driver/overview.html)).
This path does not require ARK's native-radio WPA2 patches. On 2026-11-18 the
user confirmed WPA2-Personal operation and verified ESP association on their router.

A separate [Kethen/wpa2psp release](https://github.com/Kethen/wpa2psp/releases/tag/2025-06-25)
exists, but its author targets 6.61 PRO. Its
[source](https://github.com/Kethen/wpa2psp/blob/main/wpa2.c) changes the Sony APCTL
radio stack's WPA2 recognition, RSN and EAPOL handshake. ARK-5 has the same patch
family in [Compat/PSP/src/wpa2.c](https://github.com/PSP-Arkfive/ARK-5/blob/main/Compat/PSP/src/wpa2.c).
Those patches do not provide an ESP backend and are unnecessary for the current
ESP scan/profile path. Do not bundle or enable them as an E1000 fix without a
specific demonstrated need. No upstream WPA2 code has been copied.

For an ESP hardware test, use a 2.4GHz WPA2-Personal/AES access point, select it
in native Scan and save/test it with the existing AES choice. Sony's label may
still say WPA-PSK (AES). If it is shown as unsupported, distinguish scan/UI
classification from association failure. WPA3-only and enterprise modes remain
unsupported by the current native profile interface.

## Original Browser

Browser restoration is still an investigation, not a confirmed flash requirement.
The installed PSPSDK provides a `samples/utility/htmlviewer` example launching
Sony's HTML viewer from a homebrew through `sceUtilityHtmlViewerInitStart`.
That establishes a supported entry point on suitable firmware, not E1000 support.

Check actual installed browser modules and their dependencies first. Distinguish
launching the original viewer from restoring its XMB icon/category: they have
different VSH and resource requirements. If modules/resources are missing, first
investigate loading matching user-supplied files from the memory stick with scoped
path redirection. Do not redistribute Sony firmware files in the project package.

If evidence establishes that flash changes are necessary, report the exact files,
reason and alternatives to the user and wait for their decision. No flash-writing
installer is authorized. Browser HTTPS/TLS and rendering limitations remain
independent of native WLAN connectivity; do not disable certificate verification.

## Confirmed First Probe Results

`logs/net-probe.log` records devkit `06060110`, model `10`, COMMON and INET
loading successfully, all four upper network modules appearing, and both groups
unloading with result zero. This does not yet prove network initialization or a
working virtual interface. User-mode flash directory listing returned `8001000d`;
the separately supplied `module_listings.txt` supplies the actual file inventory.

The new inventory includes 34 decrypted modules. The browser plugin imports 16
`sceHVUI_Module` functions and the utility imports 14. The supplied flash listing
omits its provider; the full 6.61 update has now supplied it for local analysis.
The remaining VSH menu gate has not been established.
Actual module names are `sceVshHVUtility_Module` and `sceVshNetconf_Module`;
the v2 probe corrects its original name guesses.

The v2 probe adds SQUARE after a successful CROSS test to initialize Sony NET/INET
and exchange a binary datagram through native UDP loopback, with bounded poll and
nonblocking receive. It leaves APCTL connection/radio attachment unrequested.
Native startup may still have hardware-facing effects. It logs before init/term,
cleans up its own resources, and retains backing modules if termination fails.
The user has now run this test: native initialization, 25-byte binary loopback,
termination and module unloading all passed. See the appended v2 run in
`logs/net-probe.log`. USB routing and interface attachment remain unproven.

## Local Firmware Source and Recovered Browser Assets

A verified full standard Sony 6.61 update is now available at
`build/firmware/661.PBP` and extracted under
`build/firmware/661-extracted/decrypted/`. Obtain further stock dependencies
from this local source instead of asking the user for individual files.

The six browser files omitted from the E1000's update table are recovered and
staged at `build/firmware/browser-runtime/`: `htmlviewer_ui.prx`, `netfront.prx`,
`libslim.prx`, `libwww.prx`, `mm_flash.prx` and `htmlviewer.res`. Ten overlapping
modules match the user's dump byte for byte. All browser-specific function
imports resolve in the local analysis set. This establishes a coherent asset
source, not working runtime redirection.

See [the milestone and reproduction commands](../milestones/native-sockets-and-browser-assets.md)
and [load-path analysis](../analysis/BROWSER_LOAD_FINDINGS.md). The staging folder
contains no launcher; do not treat it as an installed browser. No evidence yet
requires flash writes. Continue with native-interface ABI work and reversible
browser loading before attempting XMB menu restoration.


The native PSP TCP/internet download has now passed (v0.8/ESPv10). A guarded
RAM-only browser path helper and offline launcher are implemented; see
[the new test](../milestones/stock-browser-offline-loader.md). This still does
not implement APCTL profile connection or the XMB browser item. The next user
run inventories profile metadata and probes native APCTL init before opening
the original browser on a local page. No flash replacement is required to
attempt this loading path; actual rendering remains unverified.
