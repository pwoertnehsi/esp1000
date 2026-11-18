# Repository Guidelines

## Project Structure & Module Organization

- `accpower/`: PSPSDK application that activates PSP USB accessory/camera drivers, logs state changes, and supports re-arming.
- `esp_unfinished/`: experimental ESP32-S3 USB host firmware, ESP-IDF configuration, and `patch_enum.py` for changing enumeration order. Hardware behavior is explicitly unverified.
- `psp_acc_probe.py`: Linux USB host probe using PyUSB and libusb to replay camera control requests.
- `pulseview/`: captured sessions used as protocol references. Preserve original captures when adding new experiments.

## Build, Test, and Development Commands

- `make -C accpower`: build the PSP application with PSPSDK; requires `psp-config` on `PATH` and produces `EBOOT.PBP`.
- `python3 psp_acc_probe.py --help`: inspect probe options; install PyUSB and libusb before use. Actual probing requires a connected PSP and USB access permissions.
- From a prepared ESP-IDF project: `idf.py set-target esp32s3`, then `idf.py build`; use `idf.py flash monitor` for hardware validation.
- From `esp_unfinished/`, run `python3 patch_enum.py /path/to/enum.c` to patch enumeration; add `--revert` to restore its `.orig` backup.

The ESP project layout needs preparation before building: component registration is stored in `mnt_user-data_outputs_psp_host_main_CMakeLists.txt`, while `main.c` is alongside the project-level CMake file. Arrange these into an ESP-IDF main component. README references to `tools/patch_enum.py` refer to the script currently at `esp_unfinished/patch_enum.py`.

## Coding Style & Naming Conventions

Use four-space indentation in C and Python; preserve Makefile recipe tabs. Follow existing C brace placement, `snake_case` functions and variables, and uppercase constants. Keep protocol values, timing units, and hardware assumptions explicit. No formatter or linter configuration is supplied; PSP compilation enables `-Wall`.

## Testing Guidelines

No automated test suite or coverage threshold is present. Build affected firmware and record hardware, SDK version, wiring, and timestamped logs. Check accessory handshake ordering and PSP transitions from `A--` through `AC-` to `ACE`; retain failure logs and relevant captures.

## Commit & Pull Request Guidelines

Git history is unavailable in this checkout, so existing commit conventions cannot be verified. Use short imperative subjects such as `Fix accessory handshake timeout`. Describe the change, affected platform, validation performed, and unresolved hardware limitations in pull requests; link related issues when available.

## Hardware Configuration

Follow `esp_unfinished/README.md` wiring instructions: never connect board 5 V to the PSP. Keep the console on UART0 because native USB pins serve the host controller. Disable RemoteJoyLite during accessory experiments.
