# On-demand accessory power in XMB and Browser

VSH entry 0.21 and XMB bridge 0.17 reuse the tested Ad Hoc accessory-power lease
with separate VSH context checks and ownership. The kernel power implementation
is compiled from the same source; default GAME behavior and existing Ad Hoc
binaries are retained. ESP22 firmware needs no update.

Merely loading the plugin, entering XMB or opening a local browser page makes no
power request. The native adapter acquires power when Scan or saved-profile
connection processing needs its first USB helper, then waits two seconds for
ESP boot before starting USB. It retains power through helper-to-network-driver
handoff and the live browser/test connection, including recovery. A scan alone
releases power after its helper stops. Connection cancellation, failure and
normal exit release power after both USB helpers have stopped/unloaded. Failed
cleanup retains the lease; idle processing retries a failed power release.

The VSH kernel refuses normal unload while the lease or a Syscon completion is
live. Reboot-before releases the lease on VSH/game transitions. Hardware/USB
layout guards, foreign camera/storage driver refusal and completion preservation
are inherited from the tested game path. Native storage/camera launches without
a network operation do not acquire this lease. A power API success is not a
measurement of actual voltage. Events are logged to `ms0:/wifi-power.log`.

The power code's build-time context, module-type and log settings are tested in
both GAME and VSH variants. Native adapter tests check acquisition before USB
loading, failure refusal, live-helper retention, release after stop, and release
failure retry. Exact module imports/exports are inspected after building. The PSP modules build with warnings treated as errors; eight standalone C
sanitizer checks and 56 Python tests pass. Actual
VSH/browser rail-powered startup and shutdown still require hardware validation.

Test after safely ejecting and restarting VSH: verify ESP remains off in idle
XMB, run Network Settings Scan and check it powers up for scanning then turns
off, connect Browser using the saved profile and load a page, then exit Browser
and check power turns off. Repeat a connection to verify fresh ESP startup.
Use the previously successful side-rail wiring with external ESP power removed
or isolated. Check USB storage after returning to XMB. Keep GAME Ad Hoc settings
and wiring unchanged; the existing working game plugin has not been replaced.
