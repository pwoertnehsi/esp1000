# psp_host - ESP32-S3 as the PSP's USB host (camera role)

UNTESTED on hardware. Written against the ESP-IDF 5.x USB host API; your project
uses IDF 6.0.2, so the first build may need small fixes. Send me the first errors.

## What it does
The PSP (running `accpower`, state `A--`) is the USB *device*. This firmware plays
the camera: it drives VBUS like the camera does, lets the host library enumerate the
PSP, and runs the camera's request sequence with timing logs. The enumeration driver
is patched so its last stage sends the accessory handshake (`C1 01`) instead of
SET_CONFIGURATION; the app then sends SET_CONFIGURATION itself, last.

## Wiring (ESP powered from its UART board)
| ESP32-S3        | PSP breakout                         |
|-----------------|--------------------------------------|
| GND             | GND                                  |
| GPIO20 (D+)     | D+                                   |
| GPIO19 (D-)     | D-                                   |
| GPIO4 (VBUS ctl)| VBUS, through 47-100 ohm             |

* Keep the 14.7k resistor from ID to GND on the PSP-side plug.
* Do NOT connect any 5 V from the ESP boards to the PSP. Only the GPIO drives VBUS.
* Do not connect the PSP's pad rail to the ESP for now.
* Native USB pins GPIO19/20 are used for the host, so the console stays on UART0
  (that is how your dev board's USB-UART already works).

## Run
1. `python3 tools/patch_enum.py` in the project folder (finds `managed_components/*usb*/enum.c`
   or `$IDF_PATH/components/usb/enum.c`; makes `enum.c.orig`; `--revert` undoes it).
2. `idf.py set-target esp32s3 && idf.py build flash monitor`
3. On the PSP: launch `accpower` (RemoteJoyLite off). Wait for `USB state ... [A--]`.
4. Watch the ESP monitor; press SQUARE on the PSP to re-arm if needed.

## Reading the log
* `no PSP attached within 3000 ms` - VBUS not recognised, or accpower not armed, or D+/D- swapped.
* `NEW_DEV` then `ENUM ... PSP-HS: C1 01 reply 1 byte(s), first = 0x00` - the early handshake worked.
* `ENUM ... Bad transfer status` / enumeration cancelled - the PSP refused or ignored `C1 01`.
* `DEV_GONE at +N ms` - the PSP dropped the session; N tells us when.
* On the PSP screen/log: `USB state` going `A--` -> `AC-` -> `ACE` means it saw VBUS and enumerated.

## PSP side
The PSP cannot originate arbitrary packets in this mode: it is a device with Sony's fixed
accessory/camera drivers. This test only proves the link and the handshake. Real two-way
data needs a custom PSP driver, which is the next step after this works.
