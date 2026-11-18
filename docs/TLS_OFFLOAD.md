# Native browser TLS offloading candidate

VSH entry 0.22, XMB bridge 0.18 and ESP23 add an on-device TLS tunnel for the
owned 6.61 native HTTP stack. This follows the PSP capture showing TLS1.0 /
legacy ciphers / no SNI and an immediate fatal handshake_failure from the target.
Browser rendering and the existing browser exit/freezing issues are unchanged.

## Data path and verification

An exact guarded libhttp call site exposes the current HTTP connection object
before socket connect. Its original hostname (the same string used by the owned
inet_pton/resolver sequence) is taken from connection offset 0x38. The HTTPS
socket is connected to ESP 192.168.77.1:27314 instead of the remote endpoint.
Its descriptor is retained, preserving native select/poll and timeout behavior.
HTTP URLs, Host headers, paths, response bytes and redirect handling stay in
Sony's HTTP implementation; no HTML or URL rewriting proxy is used.

The private USB request contains the original DNS hostname and port443. ESP-TLS
uses that hostname for SNI and certificate-name verification, with the compiled
CA bundle, synchronized clock and HTTP/1.1 ALPN. Certificate verification is not
made optional. This build enables upstream TLS1.2; TLS1.3 is not enabled. The
ESP acknowledges success only after the verified TLS connection is established.
PSP also rejects nonzero certificate flags/error details even with a zero status.

The HTTP library's owned SSL imports map its native BIO/SSL objects to the
verified tunnel. Read/write use the existing socket with nonblocking NET calls;
SSL retry/error and readiness results reflect tunnel state. Sony object/BIO
allocation/free remains native. Unbound SSL objects cannot report successful
handshakes or silently fall back to legacy TLS. No encrypted site traffic is
sent as cleartext over Wi-Fi. Plaintext exists on the short private USB link and
inside ESP/PSP RAM, as expected when the ESP is the TLS endpoint.

The service binds only the private USB address and admits only the PSP's private
IP. It allows two concurrent connections and uses bounded streaming buffers.
Explicit proxy configurations, IP-literal identities and non-443 HTTPS ports are
refused in this initial implementation. Public CA verification is supported;
custom/self-signed certificate acceptance and offloaded certificate-display UI
are not implemented. Sony's custom application verification/pinning callbacks
are not reproduced. This is a native-browser candidate, not a claim of general
compatibility for every VSH application using the shared HTTP library.

SNTP is initialized once with networking and started after station IP acquisition.
TLS refuses to proceed with an unset clock. No trust is inferred from a shared
CDN address or reverse DNS. Blocking UDP123/DNS may prevent initial clock sync;
this is reported as an error rather than bypassing verification.

## Scope and lifecycle

Only the guarded HTTP module call site and selected import stubs are patched in
RAM. Actual version/text-size, library-table metadata, selected NIDs, expected
resolved exports, callback ownership and original call-site instructions are
checked before any write. Foreign changes fail preflight. Original SSL provider
exports and flash modules are not replaced. The MIPS thunk preserves the five
connect arguments and adds the caller's saved request/connection registers.

The shim retains slot identities across BIO/SSL ownership changes, guards
nonblocking I/O against close/reuse, and defers close during a pending tunnel
handshake. Generations reject stale BIO bindings. Capacity, cancellation,
timeout, close-before-free and free-before-close are bounded and tested. Normal
plugin unload is refused while its HTTP callbacks or tunnel state are live.
The on-demand power lease, native saved AP profiles, Ad Hoc plugin and ESP21
mapping recovery are retained. The service expires a disconnected USB session
rather than retaining upstream connections indefinitely.

## Private USB protocol v1

TCP port27314 accepts `TLS1`, uint16LE hostname length, uint16LE port443, then
1..253 hostname bytes without a terminator. The DNS-name validator bounds labels
and rejects control characters, empty labels, URI delimiters and IP literals.
Reply is16 bytes: `TLR1`, int32LE status, int32LE TLS detail, uint32LE certificate
flags. All three values must be zero before application I/O. Subsequent bytes
are the native HTTP plaintext stream in each direction.

Status -3 means no trusted clock, -4 allocation failure, -5 upstream TLS failure.
The PSP log also records underlying TLS error and certificate flags. Transport,
invalid response, capacity and cancellation failures likewise cannot produce a
verified session. Logs contain stages and numeric results, never headers,
cookies, request bodies, credentials, keys or full URLs.

## Validation and first hardware test

PSP modules build with warnings as errors. ESP-IDF builds ESP23; insecure TLS
configuration is disabled. Eight standalone C sanitizer programs and60 Python
tests pass. New tests compile actual PSP callbacks, ESP service and kernel hook
guards with mocked platform APIs. They cover partial duplex I/O, verified/error
ACKs, invalid request/host, unset clock, configured trust/SNI/ALPN, certificate
failure, capacity, cancellation, fd reuse, BIO generations and atomic preflight.
These mocks do not establish real browser ABI interoperability or a successful
public-site TLS exchange; those require the hardware test.

ESP23 flashed with verified image hashes and boots normally. FIFO_LAYOUT still
reports RX140 / PTX140+84 / NPTX224+32 and the idle watchdog counter is zero.
App SHA256: `874d8e4f8cff0c7a640bfd71858852edbfede4c70203a3dbbc08e0267f7332a0`.
Bootloader and partition images are unchanged. Prior ESP22 and PSP modules,
new binaries and build/test/flash evidence are archived in the ignored local
`artifacts/tls-offload-v01` directory.

Install both PSP modules together and restart VSH. Test the original HTTPS URL
with side-rail power and UART disconnected. Initial clock sync may add delay.
If it fails, mount PSP and preserve `ssl-offload.log`, `wifi-vsh.log`,
`native-wifi.log` and `psp-vnet.log`. `TLS_OFFLOAD_HOOKS status=1` confirms hook
installation; VERIFIED / SSL_BOUND / HANDSHAKE_READY records distinguish stages.
The earlier passive-capture marker remains disabled.

## Post-handshake read correction (bridge 0.20)

The first UART-free hardware capture showed verified handshakes, complete HTTP
request writes (226 or 271 bytes), then a single read returning WANT_READ and
immediate SSL free. One attempt received 568 bytes before a subsequent request
failed. Owned libhttp clears SO_NONBLOCK after socket connect and its HTTP read
worker treats negative reads as request failure, rather than retrying WANT_READ.
The original offload shim forced nonblocking reads without implementing that
blocking contract; 80431066 can be a later request-state failure.

Bridge 0.20 retries transient reads/writes for blocking sockets. Each NET call
remains nonblocking and holds the registry lock only for that call. Between
attempts it yields and validates the original slot generation, SSL identity and
descriptor, allowing socket close or SSL free to cancel the wait without fd
reuse. Explicit nonblocking sockets keep WANT_READ/WANT_WRITE behavior. The
PSP's four-byte socket timeout is used when set (capped at 60 seconds); unset
timeouts use a bounded 30 seconds. EOF and hard errors return immediately.
No ESP update or certificate-policy change is required.

The three TLS sanitizer tests pass, including delayed successful read/write,
native timeout expiry, explicit nonblocking behavior and close during a pending
read. The PSP bridge builds with warnings as errors. Hardware confirmation of
HTTPS pages and RSS/Browser exit is still pending. Previous logs/binary are
archived locally in artifacts/tls-offload-v03.

### Follow-up: bridge 0.21

The user reproduced 80431066 with bridge 0.20 through both launch contexts; RSS
Guide still exits cleanly and standalone Browser still hangs. The new capture
again shows successful request writes followed by WANT_READ and immediate SSL
free. Transport ends the tunnel with a PSP-originated FIN before response data
arrives, with no USB faults in that session. The previous change did not resolve
the hardware failure and its nonblocking-mode exception was insufficiently
justified for an HTTP SSL callback.

Bridge 0.21 applies the bounded wait regardless of the underlying TCP mode.
The owned HTTP SSL read call has no per-call nonblocking argument; exposing the
tunnel descriptor's mode is not a substitute for that callback's contract. The
first twelve calls also log socket mode, wait budget and elapsed microseconds,
so a remaining timeout can be distinguished from a bypass without UART. Tests
now require delayed-data success even with a nonblocking descriptor and verify
the deadline on that descriptor. The same timeout, cancellation, certificate
verification and bounded diagnostics remain. Hardware confirmation is pending.

Launch behavior is unchanged. Further inspection identifies launcher interface
slot 0x24 as the URL-launch function at launcher text+0xdfc (interface starts at
its data+0x10). RSS invokes it through content_browser with its completion
context; substituting the RSS action into Browser would only open the RSS menu.
