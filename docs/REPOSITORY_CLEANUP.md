# Repository cleanup map

This is a dependency inventory for the production path in tools/build_psp.py and
esp_link/CMakeLists.txt, not an instruction to delete files. Nothing listed here
was removed. The root Makefile's older targets and some tests still use the
experimental applications, so cleaning them up also means updating those targets
and tests.

## Keep: current runtime and build inputs

| Path | Current use |
| --- | --- |
| wifi_plugin/ | Native XMB menus, APCTL routing, TLS offload client, native USB/session helpers and VSH power integration |
| esp_link/ | Current ESP32-S3 host/router/TLS/Ad Hoc firmware |
| esp_link/components/tinyusb/ | Actual host stack, including the race-stability FIFO correction; not a disposable download/cache |
| adhoc/aemu/ | Current GAME transport, power lease and assembly imports; copied into pinned aemu at build time |
| third_party/aemu/ | Ignored pinned upstream checkout and submodules; required to build GAME modules; can be re-cloned |
| common/ | Shared protocols, profiles, packet/DNS/TLS helpers and diagnostic switch; older names do not mean unused |
| virtual_net/driver/ | Current native virtual network driver, USB transport and accessory assembly |
| virtual_net/app/native_config.c, native_config.h, native_config_abi.c, native_http.c, native_http.h | Shared setup/route/local HTTP implementation compiled into native_session or GAME shim; keep accompanying ABI headers |
| virtual_net/control.h | Shared PSP driver control ABI |
| browser_local/app/adapter.c, adapter.h, dns.c, dns.h, lookup.c, lookup.h | Shared live network facade/DNS helpers compiled into native_session |
| browser_local/driver/path_patch.c, path_patch.h, asset_device.c, asset_device.h, browser_guards.h | Compiled/included by current VSH browser runtime |
| analysis/browser_asset_matrix.json | Input to first-install browser-asset preparation; not just a historical report |
| tools/build_psp.py, install_psp.py, prepare_aemu.py, package_esp.py | Current production build/install path |
| tools/prepare_browser_firmware.py, prepare_browser_local.py, prepare_browser_signed.py, inspect_prx.py | First-install local browser assets and firmware inspection |
| tools/configure_adhoc_game.py | Optional per-title ARK compatibility settings |
| tools/esp_bridge.py, esp_bridge_host.py, esp_uart.py, read_ssl_capture.py | Optional active debugging/flash/capture tools |
| tests/, docs/ | Validation and technical history; some tests intentionally cover old probes |

On the PSP, **PSP/GAME/psp-browser/psp_vnet.prx is still used by both plugins**.
The directory name is historical. b0/ and b1/ are current native-browser assets;
do not remove them because the old EBOOT launcher is no longer used.

## Outside the current production path: archive candidates

| Path | What it is / consequence of removal |
| --- | --- |
| esp_unfinished/ | Early ESP-IDF USB host experiment and enumeration patch; superseded by esp_link with vendored TinyUSB. Current production never applies patch_enum.py |
| accpower/ | Standalone accessory/camera power experiment. Current lease lives in adhoc/aemu/esp_power.c and is reused by VSH. Preserve useful hardware evidence even if archiving the app |
| psp_acc_probe.py | Linux/PyUSB camera-handshake experiment; not part of current ESP or PSP plugins |
| psp_link/ | Original USB demo/file-transfer applications and driver; root `make psp`/`make package` still select it. test_psp_loader.py still compiles its application |
| net_probe/ | Standalone native networking probe. Native socket and probe tests still compile sources here; removing it breaks those tests until reorganized |
| adhoc/peer_test/ | Optional PSP/PPSSPP packet smoke test, not installed by the production package. Useful to separate basic transport from a game's behavior |
| virtual_net/app/main.c and standalone app packaging | Old interface/packet experiment; production uses selected native_* sources listed above, not this application |
| browser_local/app/main.c and its EBOOT launcher | Earlier standalone diagnostic browser; production now launches Sony's native VSH entry |
| browser_local/driver/main.c, code_patch.c and old launcher-specific guards | Earlier standalone browser PRX/patch path; production uses selected shared files, not the complete browser_paths PRX. Keep shared dependencies above |
| tools/package_wifi_plugin.py, package_browser_code.py, package_adhoc.py | Historical candidate packages. New production packaging is tools/build_psp.py; older Makefile targets/docs still refer to these scripts |
| tools/prepare_xmb_menu.py, collect_sdk_nids.py, make_ifhandle_guard.py, map_prx_dependencies.py | Analysis/guard generation tools; not required for a routine build with committed guards, useful for firmware-porting work |

Most analysis/ reports and older milestone/investigation documents are build-time
history, not runtime files. Keep the browser asset matrix, provenance and any
analysis needed to regenerate guarded patches. Local modules/ contains personal
firmware dumps used for old guard generation; routine production builds do not
need it, but it is not replaceable from our code-only package.

## Generated files: rebuildable, but preserve rollback/evidence you need

- `esp_link/build/`, `esp_link/build-*/` and root `build-*/`: generated IDF builds.
  Flash commands and UART bridge flashing require their expected binary files;
  deleting a build directory requires rebuilding before flashing it.
- `.o`, `.elf`, `.prx`, EBOOT.PBP, PARAM.SFO and generated archives are build
  outputs. Do not confuse generated PRXs with source files or installed copies.
- `build/*/aemu-build/`: generated patched aemu trees; upstream plus preparer
  can recreate these, while old trees also document exactly what was tested.
- `build/production*`, `build/debug*`, `build/esp-production*`: generated
  deployable packages. Keep at least the hardware-proven rollback you need.
- `logs/`, failure-capture artifacts, pulseview/ and older milestones/: not used
  at runtime. Archive original captures and useful milestones before tidying;
  they are evidence, not compiler caches.
- `.uart-bridge/`: local bridge state; stop the bridge before clearing it.
- `esp_link/sdkconfig`, sdkconfig.old, dependencies.lock: local generated files.
  The reproducible defaults and sdkconfig.production are tracked and must stay.
- `esp_link/main/wifi_credentials.h`: ignored legacy personal header. Current
  firmware sources do not include it; saved runtime profiles replaced compile-time
  credentials. Keep it private or remove it yourself.

A small buildable checkout still needs the shared portions of browser_local/ and
virtual_net/, the pinned third_party/aemu checkout, and current tools/common/.
Moving those shared sources into clearer directories would be a separate refactor.
