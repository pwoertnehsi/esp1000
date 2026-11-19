# Build, install and troubleshoot ESP1000

The supported setup is PSP-E1000, owned 6.61 firmware, ARK-5, and ESP32-S3.
Native Settings supplies saved infrastructure profiles, including WPA2. The GAME
plugin supports PPSSPP over the same LAN. Ordinary PSP-radio Ad Hoc is a different
backend and is not implemented. Browser exit was hardware-verified with 0.29;
0.30 adds selectable diagnostics without changing that fix.

Commands below run from the repository root in Linux. Production PSP output
contains our modules, GPL Ad Hoc components and a matching source archive; Sony
browser assets remain a separate local preparation. Existing installations keep
their b0/b1 assets, saved profiles and Ad Hoc config.

## Prerequisites

- Install [PSPDEV/PSPSDK](https://pspdev.github.io/installation.html), including
  the SDK libraries used by these Makefiles. This build used psp-gcc15.2.0.
  `psp-config`, `psp-gcc`, `psp-prxgen` and `make` must be on PATH. For an
  installation at /opt/pspdev:

  ```sh
  export PSPDEV=/opt/pspdev
  export PATH="$PSPDEV/bin:$PATH"
  psp-config --pspsdk-path
  ```

- Python3, Git, a native C compiler and make. Native tests use ASan/UBSan.
- [ESP-IDF v6.1](https://docs.espressif.com/projects/esp-idf/en/release-v6.1/esp32s3/get-started/index.html)
  and its ESP32-S3 tools. This repository was built with the v6.1 tag, not an
  arbitrary newer SDK. Activate its export.sh for ESP commands. TinyUSB is
  vendored and includes the hardware-verified FIFO correction; keep it.
- Clone the pinned Ad Hoc dependency once:

  ```sh
  git clone --recurse-submodules https://github.com/Kethen/aemu.git third_party/aemu
  git -C third_party/aemu checkout f6bc457eca56c4f08cad39aeac80fcc6dce6db6a
  git -C third_party/aemu submodule update --init --recursive
  ```

  The preparer checks that commit, copies it into the output directory, applies
  our adaptation, and uses its bundled CFW SDK stubs. It does not install files
  into your system PSPSDK. The upstream checkout stays unchanged and ignored.

## Build the PSP production package

```sh
python3 tools/build_psp.py --profile production --output artifacts/my-production
```

Equivalent: `make production OUTPUT=artifacts/my-production`.
Choose a fresh output directory each time; previous builds are preserved.
The script cleans shared PSP objects before compiling, avoiding reuse of a
module from a different diagnostics/backend build.

Output:

- `artifacts/my-production/psp/`: files laid out relative to the memory-stick root.
- `esp1000-psp-production.zip`: the same runtime package.
- `esp1000-source.zip`: project and adapted Ad Hoc source plus bundled CFW stubs.
- `aemu-build/`: prepared Ad Hoc tree and build provenance.

For a first Ad Hoc installation you can add `--server YOUR_PC_IPV4` to the build
command. Otherwise create `seplugins/esp-adhoc/server.txt` yourself with the
PPSSPP server's LAN IPv4 and a newline. Do not use the ESP's address. The
installer preserves existing server, nickname, hotspot and port-offset files.
An empty hotspot.txt selects the existing saved PSP profile; credentials are
not compiled into either firmware.

## Install the PSP package

Mount the PSP in USB storage mode, then:

```sh
python3 tools/install_psp.py --build artifacts/my-production/psp \
  --psp /run/media/tenshi/PLAYSTATION
```

Replace the mount path with yours. The installer verifies package hashes before
writing, backs up replaced files and PLUGINS.TXT under artifacts/install-backup-*,
and writes/fsyncs/read-verifies each replacement. It preserves personal Ad Hoc
configuration and title-specific or disabled plugin entries. It does not remove
old logs or change ARK memory/cache settings. Inspect its printed result.

For manual installation, copy/merge these package directories into the memory
stick root, preserving existing personal configuration:

- `seplugins/esp-wifi/`: four native VSH modules.
- `seplugins/esp-adhoc/`: atpro.prx and its matching kd/ modules, plus config.
- `PSP/GAME/psp-browser/psp_vnet.prx`: shared virtual network driver. Both VSH
  and GAME still use this path even though the old launcher is not needed.

Merge the lines in package `plugins-entry.txt` into `seplugins/PLUGINS.TXT`.
Do not replace your plugin list. Fresh entries are:

```text
vsh, ms0:/seplugins/esp-wifi/wifi_vsh.prx, on
vsh, ms0:/seplugins/esp-wifi/wifi_menu.prx, on
game, ms0:/seplugins/esp-adhoc/atpro.prx, on
```

Keep an existing per-title Ad Hoc entry if preferred. For a newly configured
retail title, use `tools/configure_adhoc_game.py --psp MOUNT --game TITLEID`:
it adds title-specific `highmem:auto`, disables the incompatible memory/ISO
caches for that title and backs up settings. Do not run it just to update an
already working setup.

Safely eject, leave USB mode, and **restart VSH** to load updated VSH modules.
GAME modules update on the next game launch. Test native Settings Scan, saved
profile browser use and both Circle/HOME exits, then your Ad Hoc game.
The current production profile has been built and tested locally; hardware
network/gameplay acceptance of this quiet build remains a separate check.

### Browser assets for a first installation

An existing working PSP already has these files; keep them. Obtain your own
Sony6.61 updater, place it at `artifacts/firmware/661.PBP`, and install
[pspdecrypt](https://github.com/John-K/pspdecrypt). Then:

```sh
python3 tools/prepare_browser_firmware.py artifacts/firmware/661.PBP
python3 tools/prepare_browser_local.py --assets-only
```

The second command uses the committed asset matrix and verifies original signed
PRXs against the decrypted SHA256 references. Copy **only** b0/ and b1/ from
`artifacts/browser-local/` to the memory-stick root. No flash0 writes are needed.
This mode avoids regenerating historical utility guards from local modules/.
Sony assets are excluded from the code package and source archive.

## Build and flash ESP production firmware

Activate your IDF v6.1 environment and use a separate build/configuration:

```sh
source /opt/esp-idf/export.sh
mkdir -p esp_link/build-production
IDF_COMPONENT_MANAGER=0 idf.py -C esp_link -B esp_link/build-production \
  -D SDKCONFIG=build-production/sdkconfig \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.production' build
```

Here `-B` is relative to the shell's current directory, while SDKCONFIG is
relative to esp_link. Run exactly from the repository root. The separate
SDKCONFIG prevents silently inheriting a developer build's log settings.
ESP32-S3 is already selected by sdkconfig.defaults; set-target is unnecessary.
The production overlay compiles application logging and bootloader logging at
level0. An ESP ROM boot message or panic output can still appear. This does not
change TLS trust verification, WPA2/profile handling or the FIFO correction.

Close the UART monitor/bridge before direct flashing so only one process owns
/dev/ttyUSB0. Connect the ESP's PC UART/power cable and isolate/disconnect its
PSP side-power lead while externally powered. Keep native USB for the PSP;
UART0 is the console. Never feed the PSP board5V.

```sh
IDF_COMPONENT_MANAGER=0 idf.py -C esp_link -B esp_link/build-production \
  -p /dev/ttyUSB0 flash
```

The firmware produced here is not automatically flashed by the PSP installer.
No erase-flash is needed. This uses that build directory's saved configuration.
Its generated flasher_args.json/flash_args provide the correct offsets. For a
portable binary bundle:

```sh
python3 tools/package_esp.py --build esp_link/build-production \
  --output artifacts/esp-production
```

From that bundle directory, with IDF's Python/esptool available and the bridge
stopped: `python -m esptool --chip esp32s3 -p /dev/ttyUSB0 write-flash @flash_args`.
See Espressif's [build/flash guide](https://docs.espressif.com/projects/esp-idf/en/release-v6.1/esp32s3/get-started/linux-macos-start-project.html)
and [build configuration guide](https://docs.espressif.com/projects/esp-idf/en/release-v6.1/esp32s3/api-guides/build-system.html).

## Debug flags and troubleshooting

| Setting | Production | Diagnostic build |
| --- | --- | --- |
| PSP package | `--profile production` | `--profile debug` |
| Individual PSP Makefiles | `DIAGNOSTICS=0` | `DIAGNOSTICS=1` (default) |
| C macro | `ESP1000_DIAGNOSTICS=0` | `ESP1000_DIAGNOSTICS=1` |
| Ad Hoc preparer | `--production` | omit `--production` |
| ESP SDK defaults | base plus sdkconfig.production | base sdkconfig.defaults only |

The PSP debug profile enables project diagnostics, not upstream aemu DEBUG mode:
upstream still builds RELEASE=1 in both profiles. Do not use an unadapted atpro
binary or independently flip upstream flags. Both profiles retain optimization
and identical networking, power and callback-routing behavior.

For a PSP problem:

```sh
python3 tools/build_psp.py --profile debug --output artifacts/my-debug
python3 tools/install_psp.py --build artifacts/my-debug/psp --psp MOUNT
```

Restart VSH, reproduce one clearly identified action (Circle exit and HOME/XMB
exit are different tests), then mount the PSP and collect the applicable logs:

| Log at memory-stick root | Purpose |
| --- | --- |
| wifi-vsh.log | Plugin startup, native hooks, XMB action routing |
| native-wifi.log | Scan/connect events, APCTL handlers, helper cleanup |
| psp-vnet.log | Shared USB transport and packet counters, XMB and GAME |
| wifi-power.log / esp-adhoc-power.log | On-demand accessory rail leases |
| esp-adhoc-plugin.log / esp-adhoc.log | GAME hooks, connection and port mappings |
| ssl-offload.log | Verified ESP TLS acknowledgment, errors and HTTP status |
| browser-exit.log | Guarded Sony shutdown call tracing |
| browser-thread.log / browser-sony-thread.log | Plugin and Sony thread wait snapshots |

Old files remain after installing production. Their existence does not mean
production is still logging: compare file sizes/timestamps, or archive them
before the next diagnostic run. Diagnostic records can identify peers/sites;
keep captures private until reviewed.

For deeper TLS troubleshooting, create an empty file **ssl-capture.enable** at
the memory-stick root in a PSP debug build. Reconnect and reproduce once; collect
ssl-capture.log and ssl-offload.log, then remove the marker. The capture parses
bounded protocol metadata; see SSL_CAPTURE.md and TLS_OFFLOAD.md. Production
ignores this marker and creates no SSL capture file.

For UART diagnostics, build a separate ESP debug configuration (production's
existing SDKCONFIG cannot be changed merely by changing SDKCONFIG_DEFAULTS):

```sh
source /opt/esp-idf/export.sh
mkdir -p esp_link/build-debug
IDF_COMPONENT_MANAGER=0 idf.py -C esp_link -B esp_link/build-debug \
  -D SDKCONFIG=build-debug/sdkconfig -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
IDF_COMPONENT_MANAGER=0 idf.py -C esp_link -B esp_link/build-debug \
  -p /dev/ttyUSB0 flash monitor
```

UART runs at115200. `status` gives transport/recovery counters; `tlscheck` tests
the firmware's three fixed TLS probe targets while Wi-Fi is connected. It does
not test arbitrary websites or their HTTP responses. Exit the monitor with
Ctrl+] before starting the bridge. To use this session's bridge instead:

```sh
source /opt/esp-idf/export.sh
python -I tools/esp_bridge_host.py --port /dev/ttyUSB0
```

In a second terminal, `python3 tools/esp_bridge.py --help` lists capture/command
options; see tools/ESP_BRIDGE.md. Its flash operation intentionally reads the
fixed esp_link/build paths, so use direct IDF flashing above for separate
production/debug builds unless you deliberately stage the matching three files.
Production ESP logging is compiled out, so its UART status/TLS diagnostic text
requires a debug reflash. PSP-side logs alone remain useful without UART.

After debugging, install the production PSP package and flash the production
ESP build again. Keep prior raw captures and known-good binaries locally.
