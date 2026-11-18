# First E1000 / PPSSPP Ad Hoc peer exchange

On2026-11-18 the user reported packets exchanged between the E1000 and PPSSPP
on the same LAN. PSP logs independently confirm15 outbound and15 inbound UDP
datagrams on the test's offset port13090, alongside the PRO server TCP session.

The game plugin loaded the replacement Ad Hoc modules, initialized NET / Ad Hoc /
control, joined groupESP1000 and created the PDP socket. The ESP applied the
existing saved profile and acknowledged port-map operations successfully. PDP
close, control disconnect/term, Ad Hoc term and NET term all returned zero. The
virtual driver records USB_STOP, detach, destroy and STOP_END all successful,
with no rejected packets or driver fault in its final statistics.

This validates real bidirectional PDP traffic through ESP NAPT, rather than only
a server login or local loopback. It does not establish PTP, Matching, GameMode,
Gran Turismo multiplayer or long-term disconnect recovery. The post-run ESP
status has one cumulative watchdog reset; there is no synchronized baseline to
assign that reset to this successful session. The Browser exit issue remains
separate and unresolved.

The passing runtime uses ESP20, the patched aemu build from the pinned upstream
f6bc457eca56c4f08cad39aeac80fcc6dce6db6a, the explicit E1000 utility-loader hooks
and peer-test v0.3. Existing VSH plugin entries were retained. The legacy GAME
RemoteJoyLite entry was disabled before testing.

Raw logs and private configuration are retained locally in
milestones/adhoc-first-peer-pass. Exact runtime binaries, ESP flash images and
SHA256/source manifests are preserved in artifacts/adhoc-first-peer-pass. These
ignored archives may contain user identifiers and are not release packages.

The next hardware target is Gran Turismo lobby discovery and a race between the
owned matching dumps on PSP and PPSSPP, retaining the same LAN/server settings.
Each of lobby, race start, gameplay and exit needs a separate result.
