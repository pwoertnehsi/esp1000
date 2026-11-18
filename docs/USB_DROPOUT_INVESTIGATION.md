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

## Diagnostic build: PSP v0.18 / ESP17

No recovery policy, transfer watchdog threshold or frame acceptance rule changes
are included. Enumeration observer callbacks only store metadata in a bounded
32-row ring; UART output is deferred to status reports. ENUM_DIAG phases are
0=begin, 1=delay dispatch, 2=control completion/dispatch, 3=finish. The stage
numbers follow the local usbh.c ENUM_* / ENUM_AFTER_* definitions. Phase3 result1
means success; phase2 uses TinyUSB's transfer result (0=success). Synthetic
phase2 ADDR0_DEVICE_DESC dispatch records zero bytes without reading its
uninitialized transfer length. LOCAL_SHA256SUMS records the small vendored hook;
the original imported SHA256SUMS is preserved.

Frame checks are a bitmask: 1=wrong transfer size, 2=magic, 4=version, 8=type,
16=payload length outside the fixed frame. No raw bytes or payload are logged.
ESP BULK_DIAG reports the actual endpoint numbers, current pending request ages,
last-completion ages/results/byte counts and last invalid-header check mask.
PSP USB_DIAG adds attachment/epoch state, attach/detach counters, pending request
ages, results and received header checks to the existing transport log. Buffers
and transfer scheduling are unchanged. Additional UART output can affect timing;
a disappearing fault is not by itself proof of a fix.

Controlled test: keep the ESP externally powered; select the saved connection,
leave the native browser idle for60 seconds, then load one fixed HTTP page such
as http://suckless.org/faq/ and wait30 seconds. Capture UART throughout; afterward
mount the PSP to collect psp-vnet.log, native-wifi.log and wifi-vsh.log. Record
whether the fault first appeared while idle or when loading. A secured cable /
stable external-power repeat is a separate comparison; change one variable per
run. Do not reset the ESP after a failure until its diagnostic state is captured.

## Diagnostic capture result — PSP v0.18 / ESP17

The user connected and left Browser untouched. No error appeared on screen.
The PSP log still contains initial/restored automatic HTTP requests, so this
was a hands-off test rather than a traffic-free idle test. The planned manual
page-load phase was skipped after the fault was already observed.

At ESP uptime106961ms an invalid64-byte frame fails mask02: only magic is wrong;
size, version, type and length pass their checks. At111981ms, after5008ms with
no progress, the outgoing watchdog restarts the host. The PSP reports successful
send completions and an outstanding receive whose age increases, with no logged
header or endpoint error. Wi-Fi stays associated. USB readiness recovers after
the restart, then the native session ends. The browser remaining visible is
therefore not proof the network session is still active.

Header corruption is now narrowed but not localized to sender, receiver or
controller. The diagnostic does not establish a physical cause or exclude one.
Next investigation should check sender-side header integrity around submission
and completion, together with host FIFO/channel state. Private correlated logs
are retained locally under milestones/usb-diag-v18-hands-off.

## Sender/FIFO boundary diagnostic build — PSP v0.19 / ESP18

USB_TX_HEADER reports cached header checks before submit and after completion,
a sticky byte-change mask over the16-byte header, and read-only KSEG1 uncached
header checks after writeback and after completion. Unsupported address mappings
report80000000 rather than dereferencing an unknown alias. Cumulative failure
counts retain transient errors; no raw header or payload bytes are logged.
Host tests inject sender-buffer mutation and verify detection without changing
transfer acceptance. The actual uncached alias still needs hardware validation.

The ESP's optional DWC2 receive observer only watches its own static link RX
buffer. It stores bounded metadata in the ISR; logging happens in the owner
task. FIFO_DIAG reports header checks immediately after the copy, along with
channel, count, offset, capacity and overflow classification. CHANNEL_DIAG takes
best-effort read-only register/software-state snapshots for allocated channels,
on invalid frame/watchdog and connected pre-mount status. Snapshot fields may
span interrupt progress; they are not an atomic hardware trace. No payload,
raw header or buffer address is logged. FIFO copies, controller registers,
packet acceptance and retry thresholds remain unchanged. Timing perturbation
from instrumentation remains possible; the fault is not declared fixed.
