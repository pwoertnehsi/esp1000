# RSS Guide browser launch investigation

On 2026-11-18 the user reported that RSS Channel Guide can close cleanly back
to XMB, while Internet Browser still hangs on exit. Both report 80431066 for
HTTPS. This is a useful controlled comparison, not proof of a different engine.

Inspection of the owned decrypted 6.61 modules establishes this call chain:

* RSS Guide's handler in `rss_browser.prx` passes the Sony guide URL, its length,
  the value 12 and a callback context to content_browser import NID 0B1F04E6.
* That export in `content_browser.prx` invokes the launcher plugin interface
  at offset 0x24, retaining a completion callback. It can first arrange a
  deferred launch when its prerequisite interface is unavailable.
* `launcher_plugin.prx` loads `flash0:/vsh/module/htmlviewer_plugin.prx` and its
  RCO. This is the same native HTML viewer used by Internet Browser, not an RSS
  executable containing another web engine. The VSH log independently shows
  the known HTML viewer consumers loading after RSS action 8, argument 11.

Hashes and reference offsets are in `analysis/rss_guide_launch.json`. No Sony
executable bytes are distributed. The normal Browser action remains 16,0;
simply replacing that action with RSS 8,11 would open the RSS menu rather than
reuse its URL-launching contract. The value 12 is not yet decoded into flags.
Do not substitute it into an unrelated VSH action argument.

Reusing the RSS URL launch context is a promising exit workaround. It needs
the launcher interface contract and callback lifetime matched first. This
investigation does not yet change browser launch or claim to fix shutdown.

The latest PSP TLS log reaches VERIFIED, SSL_BOUND and HANDSHAKE_READY. The
ESP's verified handshake succeeds on those attempts; earlier failed attempts
must not be confused with them. In owned libhttp, 80431066 is returned by
request-state checks, so the displayed error alone does not identify the first
failure. There was no application-I/O visibility in the first offload build.

XMB bridge 0.19 records the first twelve SSL read/write request lengths,
results and SSL error values per tunnel, plus SSL free events, in
`ms0:/ssl-offload.log`. It records no stream contents, URLs or credentials and
requires no UART connection. Logging happens outside the socket registry lock.
Certificate verification remains mandatory. ESP23 needs no firmware update.

The PSP bridge builds with warnings as errors; the three TLS host test programs
pass under ASan/UBSan, including positive partial application reads/writes and
existing failure/cleanup tests. A real browser retry is still required to
identify the post-handshake failure.

## Close-sequence comparison (VSH entry/bridge 0.23)

Further inspection shows launcher URL startup explicitly locating music, RSS
or photo owner plugins and arranging asynchronous owner callbacks. A bare call
to that entry with mode12 and no RSS owner does not reproduce the working path.
The normal Browser entry has not yet been redirected; the shutdown cause and
an equivalent owner/return setup remain unresolved.

The next diagnostic intercepts six exact internal calls in the owned HTML viewer
close function. It records entry (phase0) and return (phase1) to
ms0:/browser-exit.log, forwarding the original scalar arguments and return value.
The six stages are calls at 29c4,29cc,29d8,29e4,29ec,29f4 to
d7a8,d834,1c3c,1bd4,4cec,4c90 respectively. A logged return value from an internal
void operation is not an error code. It is retained only as diagnostic data.

Module version, size, user text ownership, six original call instructions,
delay slots and close-function anchors are checked before any patch. Conflicting
changes fail the whole preflight. Callback addresses belong to the resident
bridge; unload is refused while the patched HTML viewer remains alive. Menu
control ABI4 requires the kernel and user modules to be installed together.

The diagnostic changes no launch mode, source URL, teardown ordering, security
prompt or certificate policy. A clean RSS close and a stalled standalone close
must be compared before claiming a lifecycle fix. Trace files may also remain
empty if the close attempt blocks before this particular close function.

The targeted 15 tests pass under ASan/UBSan, including foreign-call refusal,
atomic preflight, delay-slot preservation, idempotency, live-owner unload refusal
and forwarding the original exit arguments/return. Both PSP modules build with
warnings as errors. Hardware comparison remains pending.
