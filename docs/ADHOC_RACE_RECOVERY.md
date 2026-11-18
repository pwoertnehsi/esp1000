# Gran Turismo race: reconnect recovery candidate

The user reports successful car transfer but a PSP-hosted multiplayer race
stalls at “Loading data...” for roughly one to two minutes. PPSSPP reports
`AM: Data from Unknown Port [ESP-1000]:40655 -> 1 (10000)`.

The preserved logs in the ignored local directory
`milestones/adhoc-race-failure-20261118` show initial bidirectional UDP Matching
on physical port 10001, followed by USB-ready transitions to down/up twice in
the latest virtual-driver session. The final counters are five attach events,
three detach events, 405 transmitted frames, 84 received frames, no rejected
frames or driver fault, and clean driver teardown. Port-map requests returned
success, but older PSP logs omit their operation and port.

[PPSSPP's Matching implementation](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceNetAdhocMatching.cpp)
subtracts the configured offset for the displayed port and warns when the
sender differs from its Matching port. With offset 10000, this report is
consistent with an external UDP source port of 50655 rather than 10001.
It does not by itself establish the reason for the USB interruptions or prove
that port remapping is the only race failure.

ESP20 clears static mappings and disables NAPT on every USB detach. Disabling
the last NAPT interface frees its TCP/UDP state as well as its mapping table.
On reattach the game still owns its sockets, so it does not reopen their maps.
Subsequent UDP can receive a random external source port, while established TCP
traffic loses its translation state. This is a concrete recovery defect.

ESP21 blocks USB forwarding immediately on link-down, discards queued frames
and ARP state, removes active static mappings, and retains their intent and
NAPT connection tuples for ten seconds. A quick reattach restores static maps
before accepting frames and retains TCP tuples. A longer interruption clears
intent and disables NAPT. Repeated down notifications do not extend the window.
This cannot guarantee recovery if the game or peer already timed out.

Repeated map requests now return success without re-adding an installed entry.
The bundled lwIP implementation can duplicate an existing mapping when adding
it again; preventing duplicate calls avoids leaked table entries. A LAN address
change replaces the old entry. The PSP shim clears old mapping intent when a
new Ad Hoc control session starts and records operation, protocol and port with
its result. Accessory power code is unchanged. The user separately confirmed
that its game-only activation behaves as expected.

Validation: ESP-IDF build, all seven aemu components, eight standalone C
sanitizer checks and 54 Python tests pass. New checks compile the actual router
transition functions and the real mapping service, including an additional run
against the installed lwIP port-map implementation. They exercise brief and
expired reconnects, duplicate down notifications, repeat requests, address
change, explicit clear and mapping-table cleanup. Hardware race validation is
pending. Bootloader and partition images are unchanged; ESP21 app SHA256:
`7b4916b44239ecb77e209cf6af8a8eab49b203bda1aeab1937f2bd09b2c7e013`.

Retest with the PSP hosting, the same game dumps and unchanged PPSSPP settings.
First try a race without deliberately disturbing USB. If it stalls, retain the
exact PPSSPP message and mount the PSP for its richer mapping log. A separate
brief-unplug test can then verify recovery; passing that alone does not prove a
race works. Do not erase the known successful car-transfer baseline.

## ESP21 race loaded, then stopped

The next user test reached the rendered track and opponent car, but the user
could not accelerate and the connection failed shortly afterward. Its PSP logs
were preserved in the ignored `milestones/adhoc-race-loaded-drop-20261118`.
They confirm successful mappings for UDP 10001, UDP 30125 and TCP 30000, and
incoming traffic on both race ports. A control TCP connection also recovered
from an early USB interruption, supporting the intended ESP21 recovery behavior.

Later, PSP USB receive requests remain pending for approximately five seconds
while PSP-to-ESP completions continue. Detach/reattach follows repeatedly, and
one receive completes with 44 bytes instead of the fixed 64-byte record. The
session ends with 432 transmitted / 133 received frames, zero packet rejection
or driver fault, four detach events and clean driver teardown. Mapping removal
then fails as transport readiness disappears. These observations identify a
remaining USB transfer stall but do not distinguish cable, power, controller,
record-framing or endpoint handling as its cause. ESP status collected after
its power cycle contains no previous-run controller history. A synchronized
externally powered UART capture is the next diagnostic; no timeout increase or
speculative controller change has been applied.
