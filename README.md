# PSP-E1000 ESP32-S3 Wi-Fi Adapter

ESP v13 resumes externally powered networking with completion-driven USB service,
160 MHz CPU and modem sleep disabled. User confirms markedly more responsive
browsing; reconnects remain unresolved. See the
[throughput experiment](milestones/esp-usb-service-v13.md). The earlier
[reduced-load startup experiment](milestones/esp-soft-start.md) did not resolve
the user-reported collapse of the PSP accessory supply under ESP load.

**Verified on PSP-E1000 / 6.61 ARK-5:** native PSP TCP/HTTP over USB and ESP
Wi-Fi/NAPT saved a **3961-byte NeverSSL page** to `ms0:/native-http.html`, followed
by complete clean XMB exit. UDP and local TCP checks also passed. See the
[native internet milestone](milestones/native-internet-routing.md).

The original [ESP HTTPS/file-transfer demo](milestones/wifi-html-download.md)
remains preserved. Stock APCTL/profile setup and browser restoration are the next
integration steps; see [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md).

## Current implementation

- `psp_link/app/`: user-mode EBOOT loader for ARK/CFW with `kuKernelLoadModule`.
- `psp_link/driver/`: kernel USB driver and test UI. Registers accessory types
  through `usbacc`, starts the accessory base driver, and adds a vendor interface
  with bulk IN/OUT endpoints. It does **not** start camera/microphone streaming.
- `esp_link/`: ESP-IDF 6.0.2 firmware using a vendored TinyUSB DWC2 host stack.
  Enumerates normally, discovers the custom interface, and exchanges packets.
- `common/`: shared frame format and validated endpoint discovery.
- `tools/`: ESP packaging and UART command/log capture.
- `accpower/`, `esp_unfinished/`, `psp_acc_probe.py`, `pulseview/`: retained earlier
  experiments and captures.

PSP is the USB **device**, ESP is the **host**. The new profile uses the accessory
type-registration API to request power while owning the application data path.
The user measured 3.3 V on the side rail while this app ran; startup, clean
exit and bidirectional packets have been verified on the PSP. The old enumeration patch
is **not required or applied** to this firmware. A failed camera-mode handshake
on the old firmware is not evidence that this custom profile works or fails.

## Wi-Fi → HTTPS → memory stick (v5, hardware verified)

The ESP connects to the configured access point, obtains time through SNTP,
and fetches **https://pspdev.github.io/** with CA-verified HTTPS. The HTML response
body is cached on the ESP (maximum 128 KiB), then automatically sent when a v5
PSP receiver announces itself. No PSP network-stack replacement is needed for
this first application-level download.

The PSP writes **`ms0:/pspdev.html`**. It first writes `pspdev.html.part`, verifies
the complete length and CRC32, then publishes the final file. Each 44-byte binary
chunk has an acknowledged offset; retrying a lost ACK does not duplicate data.
A failed or interrupted download does not replace an existing completed page.
An older completed file is temporarily kept as `.bak` while publishing a new one.

- Install **both** updated files from `artifacts/psp/PSP/GAME/psp-link/`.
- Keep RemoteJoyLite disabled and retain the verified 10 kΩ VBUS feed.
- Launch PSP ESP Link and leave it open. The first transfer is automatic.
- TRIANGLE requests another copy of the ESP's cached page.
- Success: PSP logs `FILE SAVED ms0:/pspdev.html`; ESP logs `FILE COMPLETE`
  only after the PSP acknowledges the final verified commit.
- CIRCLE/HOME retains the v3 clean exit behavior.

The ESP14/native Settings candidate takes SSID, security, key, IP and DNS from
Sony's PSP network profiles over USB. It starts unconfigured; no credential
header is compiled and the PSP registry owns saved profiles. ESP station
configuration remains in RAM. Connect through native Infrastructure Settings
after ESP power-up before using the standalone browser. See
[wifi_plugin/README.txt](wifi_plugin/README.txt) for candidate installation,
validation and remaining limitations. Earlier ESP13 archives retain the previous
bench configuration independently. UART `status` reports connection/IP and
transfer progress without credentials. The scoped bridge commands are unchanged.

## Build and package

Requires PSPSDK on `PATH`, ESP-IDF 6.1 (current bench), and a native C compiler/Python 3 for tests.

```sh
source /opt/esp-idf/export.sh
make test
make package
```

`make psp` builds and packages just the PSP app. `make esp` builds just the ESP.
The USB host sources are local; no component-registry access is needed.

Output:

- `artifacts/psp/PSP/GAME/psp-link/{EBOOT.PBP,psp_link.prx}`
- `artifacts/esp/`: complete flash bundle, including `flash_args`.
- `artifacts/psp-link-test.zip` and `artifacts/esp-link-test.zip`: the same bundles
  as archives. The PSP archive includes the `PSP/GAME/psp-link/` directory tree.

Builds passed with the real PSPSDK and ESP-IDF 6.0.2. `make test` checks wire
layout, packet bounds, endpoint discovery/malformed descriptors, UART
capture against a pseudo-terminal, and the real launcher’s CIRCLE/HOME exit
ordering with mocked firmware calls. Leak detection is disabled for the native
sanitizer run because this task environment runs under ptrace; address and
undefined-behavior checks remain enabled. Live ESP flashing, UART commands, and
host stop/restart have also passed through the scoped host bridge. PSP v3 startup and clean XMB exit have been confirmed on hardware. The user also measured 3.3 V on the accessory side rails while v3 ran.
Bidirectional PSP–ESP packet exchange is now verified; see the milestone log.

## Current bench connections

The verified bench setup uses **two USB cables plus a 10 kΩ VBUS feed**:

- PC → ESP board's USB-UART connector, for ESP power, flashing and UART0 logs.
- ESP native USB connector → PSP, for USB data and common ground.
- **PC/UART cable VBUS → 10 kΩ → PSP-facing native cable VBUS**. This supplies
  the host-presence signal that was missing when native VBUS measured 0 V.
  At 5 V the resistor limits this branch to at most about 0.5 mA.
- PSP-facing ID → 14.7 kΩ → GND remains installed.
- **GPIO4 is unwired.** The PSP's 3.3 V side rail is enabled by the app but is
  not supplying ESP power; both devices use their existing supplies.

The 10 kΩ resistor is **between the two VBUS nets**, not VBUS-to-GND. The user
confirmed this exact wiring when the first working link was observed.

The earlier `esp_unfinished` GPIO4/resistor wiring was a different prototype;
this repository previously assumed it was installed. Do not treat it as the
current setup or ask the user to change an imaginary GPIO4 connection.

In the original ESP firmware, `VBUS=0/1` reports **GPIO4 output**, not a measured
USB-cable voltage. Diagnostic firmware v4 labels it `GPIO4` and adds physical
root-port connection and USB event counters. `vbus off` also stops the USB host controller; `vbus on`
restarts it. With GPIO4 unwired, neither command switches connector VBUS.
Do not infer that GPIO4 low removes cable voltage. Native-port VBUS previously
read about 4.5 V unloaded and 0 V connected; its source behavior was not fully
characterized. The verified workaround is the current-limited VBUS feed above,
not direct bridging of unidentified board pads. USB VBUS and the PSP accessory
side pads are separate nets; do not connect VBUS to ESP GPIO/3V3 or side pads.

Native USB carries the host connection; logs/commands use **UART0 at 115200,
8N1**, with the board's existing UART0 pin mapping. The user confirmed the PSP-facing 14.7 kΩ ID-to-GND sense resistor is present.

## ESP flash and UART checks

When the serial device is outside the task sandbox, use the user-started
[scoped host bridge](tools/ESP_BRIDGE.md). Once its host helper is running:

```sh
python3 tools/esp_bridge.py status
python3 tools/esp_bridge.py flash
python3 tools/esp_bridge.py capture --seconds 15 --command help --command status
```

With ESP connected through USB-UART as `/dev/ttyUSB0`:

```sh
source /opt/esp-idf/export.sh
IDF_COMPONENT_MANAGER=0 idf.py -C esp_link -p /dev/ttyUSB0 flash
python3 tools/esp_uart.py --port /dev/ttyUSB0 --seconds 15 \
  --command help --command status --log logs/esp-startup-01.log
```

For an interactive terminal use `idf.py -C esp_link -p /dev/ttyUSB0 monitor`
(Ctrl+] exits). Run only one serial reader at a time. Log capture refuses to
overwrite an existing file; choose a new name for each run.

| UART command | Behavior |
| --- | --- |
| `status` | Show connection, endpoints' pending state, counters and errors |
| `ping [text]` | Send a ping; log the matched PSP pong and round-trip time |
| `say <text>` | Display text on the PSP and save it in the PSP log |
| `auto on` / `auto off` | Enable/disable ESP's two-second heartbeat pings |
| `vbus off` | Set unwired GPIO4 low and stop the host; does not switch cable VBUS |
| `vbus on` / `reset` | Restart the host with GPIO4 low for 300 ms; does not switch cable VBUS |
| `help` | List commands |

Texts are capped at 48 bytes. Frames are 64 bytes, versioned, with sequence
numbers and a `reply_to` field for pongs. Both sides answer pings. The PSP sends
its own two-second heartbeat; `auto off` affects only ESP-generated pings.
An incomplete bulk transfer triggers a host restart after five seconds.

## First PSP test — user performs these steps

1. Disable RemoteJoyLite and other USB plugins. Copy the **whole** packaged
   `psp-link` directory into `ms0:/PSP/GAME/`; both files must stay together.
2. Launch **PSP ESP Link**. Do not launch the stock camera app or `accpower`.
   Initialization should show successful register/start/activate results.
3. With the verified 10 kΩ VBUS feed and ESP firmware flashed, leave the app
   open for 15 seconds. Expected PSP flags progress from `A--` to `ACE`; ESP should
   report `LINK interface=...`, `LINK READY`, and received hello/ping/pong frames.
4. Run UART `ping test123` and `say hello PSP`. The PSP should show the text;
   the ESP should log a pong with a matching sequence number. Press CROSS once
   to request a PSP ping; SQUARE re-arms the PSP USB connection if necessary.
5. In v3 the user launcher handles CIRCLE/HOME, stops and unloads the kernel
   module, then requests XMB exit. The kernel worker no longer calls ExitGame.
   Press CIRCLE to test this path. Provide `ms0:/psp-link.log` and the
   ESP UART capture. If startup fails, provide the exact error codes displayed.

Startup, clean exit, enumeration and bidirectional ping/pong have now been
observed with the verified bench wiring. Re-run these steps when changing it. If
enumeration or power fails, diagnose the startup codes and logs before changing
timing, adding driver patches, or requesting more analyzer captures.

## References

The installed PSPSDK headers define the USB driver ABI and padded `UsbData`
layout. The [PSPSDK accessory API](https://pspdev.github.io/pspsdk/pspusbacc__kernel_8h.html)
documents accessory-type registration. Import NIDs are recorded in the
[PSP NID database](https://github.com/pspdev/psp-ghidra-scripts/blob/master/ppsspp_niddb.xml).
Vendored USB source/license details are in
[`esp_link/components/tinyusb/ORIGIN.md`](esp_link/components/tinyusb/ORIGIN.md).

Sony documents E1000 USB charging in its [USB Connection guide](https://manuals.playstation.net/document/en/psp/current/settings/usb.html).

## Micro-USB board VBUS diagnosis

Seller photos in `esp_photo.avif` and `esp_photo2.avif` show a Micro-USB
DevKitC-1-like board. The USB-C YD-ESP32-S3 jumper advice does not apply.
Before the successful 10 kΩ feed, the cable VBUS conductor was reconnected
and the ID resistor present, but VBUS measured 0 V at the PSP connection. The official Micro-USB
[DevKitC-1 schematic](https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-DevKitC-1_V1.1_20221130.pdf)
uses an isolation diode on native-port VBUS; powering UART need not supply
power out of the native connector. Exact clone routing remains unverified.
The user subsequently established communication using the 10 kΩ VBUS feed
documented above. Do not bridge unidentified pads.

The next architecture is documented in the [WLAN integration plan](docs/WLAN_INTEGRATION_PLAN.md), grounded in the [supplied-module analysis](analysis/NETWORK_MODULE_FINDINGS.md).

## Stock WLAN compatibility work

The next implementation is a separate memory-stick diagnostic. Run `make probe`
and follow [the PSP compatibility-check instructions](net_probe/README.txt).
It inventories module metadata; CROSS tests COMMON/INET loading, then SQUARE
optionally tests native NET/INET initialization and UDP loopback.
Return `ms0:/net-probe.log` for the next ABI analysis. It is not yet a WLAN plugin.

[Stock setup, WPA2 and browser integration decisions](docs/STOCK_WLAN_AND_BROWSER.md)
track the memory-stick-first approach and the remaining module requirements.

The [native socket and browser-assets milestone](milestones/native-sockets-and-browser-assets.md)
records the successful PSP UDP loopback test and the recovered 6.61 browser files.
The browser and native USB network interface are still under implementation.

## Virtual interface implementation

`make virtual` builds a separate [native interface test](virtual_net/README.txt).
Registration, callback dispatch and clean teardown have passed on the PSP.
v0.2 stalled during address configuration. v0.4 passed native ARP and all eight UDP packet tests on PSP, including clean
teardown and XMB exit. USB/ESP packet forwarding is the next integration step. See [the milestone](milestones/virtual-interface-registration.md).
This lab does not use USB or ESP Wi-Fi.


`make usb-virtual` builds the separate [USB packet test](virtual_net/USB_README.txt)
for PSP-E1000 / 6.61 ARK-5. Native ARP and eight UDP cases passed through USB with ESP firmware v8;
[the milestone records the results](milestones/usb-native-packet-lab.md). The install
folder is `PSP/GAME/psp-vnet-usb`; the existing local test stays available.


The [native internet test](milestones/native-internet-routing.md) adds ESP lwIP
routing/NAPT and a PSP-originated TCP/HTTP download. Install the current USB test
package, wait for USB/Wi-Fi/target readiness, then TRIANGLE to save
`ms0:/native-http.html`. PSP v0.7 / ESP v9 hardware validation is pending;
SQUARE retains the packet regression. Flash and saved profiles remain untouched.


v0.7's native TCP attempt crashed after route setup. The current v0.8 corrects
RX header alignment and requires a local TCP check (TRIANGLE) before enabling
the internet test (START). Use ESP firmware v10 and both updated PSP files;
native UDP, local TCP and internet HTML save have passed on PSP.


`make browser` builds the separate [ESP-backed browser test](milestones/browser-esp-session.md) (v0.8).
Personal installation requires both `PSP/` and `b0/` from
`artifacts/browser-local/`; the code-only ZIP omits Sony firmware assets.
The v0.1 run passed native APCTL initialization, asset redirection and cleanup,
but reached a network prompt. v0.2 passed the ESP-backed APCTL session and native DNS on hardware. v0.3
bypasses four browser-local E1000 rejection branches in RAM. v0.4 adds a
read-only browser asset device for native module loading; rendering still
requires PSP verification. v0.5 adds bounded, cancellable startup DNS checks.
Use ESP v11, both cables and the configured hotspot; press CROSS, then SQUARE.

Browser v0.6 requires both `PSP/` and the new `b1/` directory from the private
`artifacts/browser-local/` folder. Keep `b0/` for its browser resource. v0.5 passed
startup and cleanup but hit a module partition mismatch; v0.6 tests verified
original Sony module files. Rendering remains pending PSP verification.

Browser v0.6 rendered the local page using original Sony modules, but exit and
internet stalled. v0.7 tests normal-close sequencing, shared-connection lifetime,
automatic TCP source addressing and packet logging outside the forwarder.
TRIANGLE opens the file, START opens ESP HTTP, SQUARE opens internet. Keep the
existing b0/b1 assets; only PSP/ needs replacing for this build.

Browser v0.7 passed file/ESP HTTP rendering and clean exit on hardware. Internet
transfers still hit an ESP USB watchdog reset. v0.8 separates USB service from
native network RX/TX workers; hardware validation is pending. SQUARE tests
`http://suckless.org/faq/`; keep the current ESP firmware and b0/b1 files.

Browser v0.8's [first internet-page milestone](milestones/browser-internet-first-page-v08.json)
is verified: Suckless FAQ rendered and the browser/network stack exited cleanly.
The tested code snapshot is `artifacts/browser-test-v08-code.zip`; archived logs
and hashes preserve it. Larger transfers still show USB reconnects, and startup
can be slow. Bulk stability and stock profile/VSH integration remain open.
