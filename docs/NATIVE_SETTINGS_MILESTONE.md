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
