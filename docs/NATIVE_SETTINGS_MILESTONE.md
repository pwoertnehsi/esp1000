# Native network settings hardware milestone

On 2026-11-18, the PSP-E1000 user confirmed that plugin v0.14 with ESP15 firmware
showed real access points in Sony's native Infrastructure Scan, passed native
Test Connection, and reliably used the saved connection in the browser.

Sony profiles supply the SSID, security, key, IP and DNS settings. The ESP boots
unconfigured and keeps the applied configuration in RAM; credentials are not
compiled into the firmware. Use external power for the ESP during these tests.

These are user-confirmed hardware results. The attempted UART capture lost its
connection and does not independently establish the successful transaction or
explain the preceding intermittent scan failures. Repeatability and USB storage /
camera coexistence with this backend need further validation.

The browser still launches through a standalone application: its black launch
transition and VSH restart on exit remain to be fixed. The desired behavior is
the stock browser overlay over XMB, with no VSH restart. A stock four-digit
restriction prompt, when configured, is desirable but optional for this work.
Ad Hoc / Game Sharing radio and legacy online service backends remain unfinished.

## Resident browser candidate v0.15

The XMB Browser / Online Manual now forward to Sony's original ExecuteAction.
The shortcut queues that action on the user bridge instead of LoadExec. A kernel
asset helper redirects the exact missing module paths and browser resource
prefix in RAM, retaining Sony's load-to-block allocation behavior. No flash write,
model spoof, custom graphics loop or ExitGame is used by the new path. Sony's
existing parental-control flow is preserved; the four-digit prompt is untested.

Exact imports in the owned 6.61 browser plugin, UI and engine are routed to the
ESP adapter, alongside the existing netconf hooks. All metadata, NIDs, callback
ownership and stub states are checked before any import is changed. Weak imports
are rebound when the APCTL provider loads. The transport remains active while
the native browser owns the connection after netconf closes, and is released on
disconnect. Unrelated APCTL clients are not redirected.

Install the v0.15 kernel and user bridge together. ESP15 and the existing private
browser assets are unchanged. Hardware acceptance still requires opening and
closing the browser without a black transition or VSH restart, reopening it,
checking saved-profile browsing, then USB storage and camera coexistence.

## Native browser launch/exit passed; HTTP consumer correction v0.16

The user confirmed that v0.15 opens and closes the browser cleanly, with local
file pages working. Remote pages failed immediately with 80410A05 after choosing
the saved working access point. Owned 6.61 libhttp imports sceNetApctlGetInfo
independently of the three browser consumers covered by v0.15; it was reaching
the original APCTL instance, whose GetInfo returns 80410A05 when disconnected.

V0.16 adds that exact HTTP import to the existing guarded ESP facade and refreshes
already-loaded consumers when the user bridge registers. It leaves the APCTL
provider itself unchanged. Native hook ABI3 requires installing the kernel and
user bridge together. The diagnostic consumer mask records successful bindings:
bit0 browser plugin, bit1 UI, bit2 engine, bit3 HTTP. It records observations,
not current module lifetime. Firmware provenance is in
analysis/native_http_apctl.json. Native launch/exit is confirmed; the HTTP fix
still needs hardware verification. ESP15 and the saved profile are unchanged.

## Native remote browsing passed — 2026-11-18

The user confirmed that v0.16 worked, including remote pages, after the HTTP
consumer correction. Native browser launch/exit and saved-profile Internet
browsing are hardware-confirmed. The user attributes incidental connection
errors to the temporary physical wiring; these alone do not establish a new
software regression. Next priority is WPA2 validation, then Ad Hoc.

## WPA2 confirmed; connection-status and recovery candidate v0.17

The user confirmed WPA2-Personal operation by observing the ESP associated on
their router. The ESP backend therefore needs no separate ARK WPA2 extension.
No station MAC or other personal network identifiers are recorded here.

Display Connection Status crashed. Inspection of owned 6.61 firmware established
that Sony profile/scan security4 (AES) maps to APCTL GetInfo security3, while our
native adapter incorrectly returned4. Sony browser UI at3d9fc queries field4;
at3da24..3da2c it indexes the four-entry label table without a bounds check.
V0.17 fixes the conversion (0,1,2,3,4 -> 0,1,1,2,3), rejects invalid profile values,
and tests actual callback field boundaries and browser indexing with sanitizers.
An independent owned-firmware test verifies the native mapping instructions.

The user also reports that USB failures can require pressing ESP reset. Cause is
not yet established. ESP16 covers a gap in recovery: an identified PSP can remain
physically connected without finishing enumeration, outside the outgoing-transfer
watchdog. After15 seconds it restarts the host, with up to three attempts. Retry
state survives the short artificial disconnect during host restart; a real
unplug or successful mount resets the budget. Unrecognized devices, healthy idle
mounted sessions and disabled ports are excluded. This preserves the ESP's
RAM-only Wi-Fi configuration, unlike restarting the entire chip. It does not
cover failures before PSP identification or prove that all reset-button cases
are resolved. Neither correction has hardware acceptance yet. Ad Hoc follows
these stability checks.
