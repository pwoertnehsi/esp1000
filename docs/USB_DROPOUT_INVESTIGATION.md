# USB dropout investigation — 2026-11-18

Controlled comparison used PSP plugin v0.17 and ESP16, with no firmware changes
during capture. The user tried reconnecting in the failed state, then performed
two manual resets and confirmed that the second connected before dropping again.

Without a reset, the ESP remained associated with the router while USB was
unmounted, physical connection status was present, and transfer events stopped
advancing. The recovery state reported known_psp0, so the current recovery policy
could not retry this pre-identification failure. A first manual reset also had
an initial enumeration failure before a full device descriptor. A later attempt
reached the normal PSP link handshake and profile application.

The following capture starts after another reset; its uptime must not be joined
to the previous capture as one continuous timeline. It shows a working mounted
link, router association and real Internet HTTP traffic. At ESP uptime39451ms,
an invalid64-byte USB frame is reported. At44461ms the outgoing-transfer watchdog
fires after5004ms without progress, on a packet fragment. It restarts the host;
USB re-enumerates at45031ms and exchanges a fresh HELLO. The PSP disconnects at
47491ms. Router association remains present throughout.

The PSP log also shows successful local TCP, DNS and remote HTTP, followed by
frozen USB completion counts, readiness dropping and recovering, then clean
session teardown. No packet fault or logged PSP endpoint error establishes the
initial cause. The native session treats lost link readiness as fatal, so host
recovery can restore USB after the active network session has already ended.

This identifies the failure sequence, not its root cause. The invalid frame may
be a symptom rather than the initiating event. Cable, controller and software
causes remain unresolved. Next investigation should capture enumeration progress
before identification and frame-header validation failures without logging
payloads, then assess endpoint recovery and session lifetime together. Do not
infer that WPA2 authentication failed or silently extend all timeouts.

Private raw UART and PSP evidence is retained in the ignored local milestone
directory esp16-dropout-comparison. No SSIDs, MAC addresses or private IPs are
included in this document.
