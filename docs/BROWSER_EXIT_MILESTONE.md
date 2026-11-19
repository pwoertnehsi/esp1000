# Native browser exit milestone — 2026-11-19

The user confirmed both Circle exit and HOME/XMB exit work with VSH entry0.29
and user bridge0.26 on the PSP-E1000 running 6.61/ARK-5. The fix is commit
8619c95, `Route browser UI disconnect callbacks through the ESP backend`.

Sony HVUI's Disconnect and GetState imports were routed to the ESP facade,
but its private APCTL handler registration/deletion imports were still routed
to the original WLAN provider. HVUI waited for a disconnect acknowledgment
from that subscription even though the ESP transport had finished cleanup.

The fix routes the private pair, NIDs 0xa7bb73df and 0x6f5d2981, through the
same validated facade handler system. Sony receives its normal event5 with
nextState0 and completes shutdown. It requires no fabricated completion flag,
forced thread termination, ESP firmware update or change to browser launch,
TLS verification or accessory power timing.

Validation: kernel build with warnings as errors; 17 targeted native/XMB/VSH/TLS
tests, including private-import patching and disconnect delivery after deleting
a separate temporary handler; installed PRX readback verified. The user then
reported: “both xmb and circle exits worked.” This confirms the reported exit
failure is resolved in that hardware test; it is not a claim of exhaustive
browser or cable-recovery stability testing.

Firmware offsets, the failure captures and earlier unsuccessful investigations
are documented in RSS_BROWSER_INVESTIGATION.md. Existing diagnostic capture is
still present; no new diagnostic-only build is required for this milestone.
