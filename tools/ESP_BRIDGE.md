# Scoped ESP host access

The task keeps its workspace sandbox. A helper you start in your normal host
terminal opens only the serial port chosen at launch. No network listener,
whole-device-directory mount, sudo, or Full access setting is needed.

From your host terminal:

```sh
cd /path/to/esp1000
source /opt/esp-idf/export.sh
python -I tools/esp_bridge_host.py --port /dev/ttyUSB0
```

Leave it running. `Ctrl+C` stops access. Close other serial monitors first.
The ESP-IDF Python environment supplies the installed `esptool` package.
The helper prints each operation it handles.

The task can then run:

```sh
python3 tools/esp_bridge.py status
python3 tools/esp_bridge.py flash
python3 tools/esp_bridge.py capture --seconds 15 --command help --command status
python3 tools/esp_bridge.py capture --seconds 10 --command 'ping hello'
```

Only three operations are supported: device presence, 115200-baud UART capture
with an allowlist of this firmware's commands, and flashing the three fixed
ESP build binaries at fixed offsets. Capture is limited to 60 seconds and
256 KiB of output. UART commands cannot choose files, programs, or devices.
Flashing snapshots `esp_link/build/bootloader/bootloader.bin`,
`partition_table/partition-table.bin`, and `psp_link_host.bin`; it never executes
project scripts or reads a workspace-provided `flash_args` command line.

Requests and replies are JSON files in `.uart-bridge/` (mode 0700, ignored by
Git). They contain logs and are accessible to this OS user. This is a capability
granted to processes able to write that directory, not authentication between
processes running under the same user. Only start the helper for a project you
intend to let flash this ESP. Firmware can change device behavior, and the
allowed UART `vbus` and `reset` commands affect the ESP's existing PSP connection.

No PSP-side tests are performed by this helper. It does not change Linux device
permissions or Codex configuration. After startup, edits to the helper on disk
do not change the running process; restart it only after reviewing changes.
