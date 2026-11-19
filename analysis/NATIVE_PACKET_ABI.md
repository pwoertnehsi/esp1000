# Native Interface and Packet ABI: 6.61

Source: supplied decrypted ifhandle/WLAN/NET/INET images, plus the successful
v0.1 hardware log. Addresses below are **pre-relocation**, not patch addresses.
No Sony implementation code is copied into the driver. The implementation calls
Sony's own exported allocator, interface and queue services.

## Interface Lifecycle

The external record is 44 bytes. Word zero points to a 320-byte internal object
allocated from the native NET pool. Offsets `8/c/10/14` are up/down/send/ioctl
callbacks; offsets `18/1c/20` are TX head/tail/count. Create registers the external
record; attach queues an internal-object notification to Sony's NET thread and
waits for its acknowledgement. Detach similarly waits and flushes queues. Destroy
releases semaphores, internal allocation and registry slot, without clearing the
external record's stale word zero. Our lifecycle clears it only after success.

Native user-facing ioctl/up/down entry points find an external record by comparing
its word zero with the incoming internal pointer. Callbacks receive the external
record. The ioctl callback also receives a command, request pointer (null for
unknown command values), and a local pair of validated nested pointers.

`F94BAF52` at `a94` stores callback completion result at internal `10c`, then signals
the semaphore at internal `100`. In this firmware its operation argument is
ignored by the body. Up/down complete through this native helper; the local test
fixture acknowledges only its own virtual link, never ESP or AP state.

## Packet Objects and Ownership

| Offset | Meaning supported by allocator/free/queue call sites |
| --- | --- |
| `00` | Next buffer segment in the same packet |
| `04` | Next complete packet in a queue |
| `08` | Segment data pointer |
| `0c` | Signed segment data length |
| `10` | 16-bit native object type |
| `12` | 16-bit flags; bit 1 means packet-header metadata |
| `14` | Receive interface in a packet-header object |
| `18` | Complete packet length in a packet-header object |

`59F0D619` at `19b4` allocates 256 bytes, sets next/next-packet to zero, copies its
second argument into type, sets flags zero and data pointer to object + `14`.
Its first input is overwritten before the allocation. Packet-header objects
reserve through `2f`. Sony WLAN additionally reserves two alignment bytes for
normal Ethernet: first data starts at `32` hex (50 decimal), leaving206 inline
data bytes; subsequent nodes use236. v0.8 corrects the previously omitted pad. It avoids external storage/refcount flags.

`F56FAC82` at `1b18` frees a chain through offset `00`, including native external-
storage reference handling where applicable. `E2F4F1C9` at `3b4` dequeues a complete
TX packet through the external record's queue, clearing its next-packet link.
The worker owns that returned chain and frees it once after copying bytes.

`C28F6FF2` at `ac0` hands RX to helper `14e8` with Ethernet address-family value 6.
It applies drop/special-frame filters, writes the internal receive-interface
pointer, queues accepted data to the NET thread, or frees rejected data. Ownership
is transferred on every path; the caller never frees a submitted chain again.

The bounded host-testable buffer adapter rejects negative/oversized lengths,
packet-length inconsistencies, unreadable ranges and chains longer than 32 nodes.
Actual kernel accesses accept only positive PSP user-region addresses received
from the native allocator/queue. That region check is not an allocation map:
metadata provenance and lifecycle ownership remain essential. No remote packet
may choose native addresses.

## Private Address Configuration

INET export `sceNetInet_lib:4F68DB0E` at `dac` takes interface name, a four-byte
IPv4 address pointer, broadcast pointer and netmask pointer. It invokes address
removal, fills a 64-byte alias request and submits `8040691a` through its socket
ioctl path. Its name/address/broadcast/netmask placement follows the disassembled
stores, not a guessed emulator prototype. NET's Ethernet callback subsequently
handles address/up operations using the native ifhandle up callback.

v0.2 stalled inside that wrapper, before configuration returned. v0.3 instead
submits the same 64-byte alias layout directly through guarded ioctl export
`AEE60F84` at `8848`, omitting address removal. This is hardware-unverified.
Native TX dequeue signals NET event 8 even when empty; v0.3 checks TX head first
to avoid repeatedly waking the native event loop.

The actual interface name supplied by native WLAN is `wlan`, not automatically
`wlan0`. The first virtual test uses the independent name `usb`. Stock APCTL
selection and replacement of the physical WLAN attach path remain separate work.

## Boundaries Still Unproven

The registration/callback/lifecycle path passed on PSP. v0.2 address configuration stalled. v0.3 packet construction,
address configuration, native ARP and UDP receive still require hardware results.
USB framing, flow control, ESP forwarding/NAPT, DHCP, stock APCTL/profile setup,
game-wide CFW lifetime, suspend/resume and browser restoration are not implemented
by this local packet fixture.

## v0.3 Hardware Result and v0.4 Fix

The latest PSP log reaches `CONFIG_ALIAS_BEGIN`, then reports up=1, ioctl=2,
command=2, pending=1. The stalled job's wait UID `03e24c79` exactly matches the
interface completion semaphore. Native thread abort returned zero but did not
unblock that wait, so clean exit still failed.

NET disassembly at 8260–8274 increments internal108 before dispatching address
ioctl2 (ioctl3 follows the same contract at 823c–8254). Our callback returned -1
without completing either. v0.4 completes those two address notifications once
through F94BAF52; the local fixture requires no hardware programming. Wrong
records and unsupported commands remain rejected. Host regression verifies
completion count, command, record and result. PSP behavior remains unverified.
The failed v0.3 files/log are preserved in build/virtual-net-v03-failed.zip.

## v0.4 Native Packet Path: Passed on PSP

The latest `logs/psp-vnet.log` confirms address assignment returned zero and
pending completions returned to zero. Native ARP resolved the local fixture,
and all eight UDP binary payloads passed, exercising Ethernet lengths
64/65/68/208/209/512/1500/1514. Counters: TX9/RX9, rejected0, fault0, bad_record0.
Detach/destroy, module stop/unload, INET/NET termination and utility unloading
all succeeded; the log ends at XMB exit and the user confirms graceful exit.
This verifies the address-ioctl completion fix on hardware.

Passed source/binaries and the isolated run log are archived in
`build/virtual-interface-packets-v04.zip`; hashes are recorded in
`milestones/virtual-interface-packets-v04.json`. This remains a local fixture,
not ESP forwarding. The next gate is carrying these native Ethernet frames over
the existing USB link to the ESP, first with the same bounded echo test before
routing internet traffic. Stock APCTL, saved profiles and browser restoration
remain outstanding. No flash files were changed.

## USB Ethernet Lab v0.5: Awaiting PSP Test

`make usb-virtual` builds a separate `build/virtual-net-usb-test.zip`, installed
as `PSP/GAME/psp-vnet-usb`. `make virtual` retains the local fixture; both targets
clean before changing backend flags. USB EBOOT/PRX use a distinct startup magic.
The default v0.4 proven package/archive remains available. ESP firmware v6 adds
an opt-in Ethernet capability handshake while retaining the HTML receiver.

The USB build copies native TX into bounded byte records and keeps native TX
queued under backpressure. RX records become fresh native mbufs; USB never
supplies native addresses. Packet type12 carries id(u32), total(u16), offset(u16),
CRC32(u32), then up to36 bytes. Ordered64-byte bulk transfers are pipelined without
per-fragment ACKs. Maximum1514 bytes; one buffered packet per direction plus the
in-flight transfer. CRC/sequence/length errors discard partial packets. A new
start replaces incomplete state; disconnect resets state and readiness.

The ESP host task reassembles records outside transfer callbacks and invokes
common/lab_gateway.c there, then sends framed replies. This tests the exact
same ARP/eight-UDP native path through USB instead of an in-PSP responder.
It is not internet forwarding, NAPT or APCTL. Wait for USB_PACKET_READY=1 before
SQUARE; expected LAB_CLIENT_RESULT0, nativeTX9/RX9, ESP9frames/9replies/bad0.
CIRCLE/HOME performs cleanup; return logs/psp-vnet.log. No wiring changes.

USB ownership refuses an already-active bus. Partial startup unwinds owned
resources only. Stop waits two seconds for cancellation callbacks; if these
fail to return, retain module/static transfer buffers and refuse pool teardown.
Permanent native locks can still require restart. No unsafe thread killing.

Validation: both PSP variants and ESP-IDF firmware compile; six sanitizer C
suites and18 Python tests pass. New checks cover every size14..1514, CRC,
fragment ordering, queue backpressure,100k malformed records, framed ESP ARP,
actual PSP USB source with six failed startup stages, handshake,1514-byte send/
receive, reconnect reset and buffers retained on cancellation failure. The
launcher's23 lifecycle cases also compile/run for both backend variants.
PPSSPP is available from the user but has not been used: hardware USB/private
Sony modules remain the next gate. Stock browser/profile work remains pending.

## Native Ethernet over USB: Passed on PSP (v0.6 / ESP v8)

The latest PSP run in logs/psp-vnet.log confirms native ARP plus all eight UDP
binary payload tests passed through USB to the ESP fixture and back. Ethernet
sizes64/65/68/208/209/512/1500/1514, PSP TX9/RX9 rejected0 fault0 bad_record0.
logs/esp-v8-live-packets.log independently records9 complete frames,9 replies,
bad0 and no malformed USB records. USB stop, detach/destroy, module unloading,
INET/NET termination and XMB exit all succeeded. User confirmed graceful exit.

ESP v8's100ms accessory-control settling delay, bounded retry and bulk-OUT
serialization enabled bidirectional progress with the same PSP v0.6 binary.
The working run includes readiness transitions during ESP updates, followed by
stable packet delivery. Do not claim side-pad voltage was restored: it was not
remeasured. Timing/control sequencing is implicated; exact hardware causality
is not isolated. Do not change wiring based on the earlier0V side-pad report.

Passed source/binaries, isolated PSP run and UART evidence are frozen in
build/native-usb-packets-v06-esp08-passed.zip; hashes are recorded in
milestones/native-usb-packets-v06-esp08.json. The source header containing Wi-Fi
credentials is excluded; existing private firmware artifacts remain ignored.
This proves native PSP Ethernet buffers/sockets over USB. The ESP responder is
still the local fixture: internet routing/NAPT, native TCP, DHCP/APCTL/profiles
and stock browser restoration are outstanding. Next gate is ESP network
forwarding followed by a PSP-originated native socket internet download.

## v0.7 Crash and v0.8 RX Alignment Correction

The user ran TRIANGLE and the console crashed. The latest logs/psp-vnet.log
proves alias config0, default-route ACK errno0, nonblocking socket/bind0, ARP
and two74-byte TCP SYN transmissions. It ends during HTTP_CONNECT_WAIT, before
connect completion, HTTP request or any file write. Failure artifacts are frozen
in build/native-http-v07-failed.zip. No crash dump/exception PC is available;
the exact faulting instruction remains unproven.

Static analysis found a concrete ABI mismatch with Sony WLAN RX: at4434 s6=2,
4438 initializes data at mbuf+48,447c limits payload to208-2,4494 adds2 and449c
stores data=mbuf+50 before enqueue44b8. Our old first data=+48 left IPv4/TCP
headers at mbuf+62 (not word-aligned) after Ethernet removal. v0.8 uses+50,
first capacity206, later236. Tests assert IP alignment and206/207 boundaries,
with1600 binary round trips plus existing allocation/write failure coverage.
This is a plausible crash cause, not hardware proof of the complete fix.

ESP v10 adds a diagnostic TCP listener bound192.168.77.1:8080 using real lwIP TCP.
PSP v0.8 TRIANGLE now runs local TCP/HTTP and checks a known response marker;
it skips default route/Wi-Fi/NAPT and never writes an HTML file. START becomes
available only after local TCP passes, then runs the existing native internet
HTTP download to ms0:/native-http.html. SQUARE retains UDP. RX logging now shows
EtherType, protocol, TCP flags and IP alignment before native enqueue. User
performs all PSP tests. No flash/profile/wiring changes.

Host tests pass seven sanitizer C suites and19 Python tests, including23 native
HTTP/local-TCP scenarios and launcher local-before-internet gating/cancellation.
PSP v0.8 and ESP v10 hardware validation is pending; require local TCP first.

## v0.8 / ESP v10: Native Internet Download Passed on PSP

The latest user PSP log proves all eight UDP cases, local TCP/HTTP probe,
RAM-only default route (ACK errno0), remote TCP connection, PSP-originated
HTTP request, full response validation and memory-stick publish succeeded.
The provided logs/native-http.html is3961 bytes (0xf79), matching the logged
body length; title is NeverSSL - Connecting. The test used neverssl.com:80.
This is genuine native PSP TCP over USB and ESP forwarding/NAPT; ESP supplies
DNS bootstrap but does not generate this HTTP request.

RX logs confirm ip_align0, including TCP SYN/ACK and data. Native counters:
TX27/RX23, rejected0, fault0, bad_record0. Independent UART logs/esp10-result.log
records27 complete Ethernet inputs,8 fixture replies plus15 router outputs,
no malformed records or drops. USB stop, detach/destroy, kernel unload,
INET/NET termination, utility unloading and XMB exit all returned success.
The alignment correction is now validated with UDP, local TCP and internet TCP;
no exception dump exists to identify v0.7's exact crash instruction.

Exact source, PSP/ESP binaries, isolated passed log, UART evidence and saved page
are frozen in build/native-internet-v08-esp10-passed.zip. Hashes and page
size are in milestones/native-internet-v08-esp10.json. Private credential source
is excluded (firmware artifacts remain private/ignored). Preserve this baseline.

Next integration boundary: stock APCTL/profile setup and ARK-5 system-wide
interface lifetime, then browser RAM loading/redirection. DHCP, native PSP DNS
configuration, suspend/resume, disconnect stress and HTTPS remain unverified.
No flash files or saved profiles were modified for the successful test.
