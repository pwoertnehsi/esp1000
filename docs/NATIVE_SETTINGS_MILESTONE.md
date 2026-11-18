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
