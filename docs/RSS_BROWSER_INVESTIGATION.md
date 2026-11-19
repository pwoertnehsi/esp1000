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

## Relocated guard correction (VSH entry/bridge 0.24)

The first device trace contained only its build marker. Code review found the
stage0 delay-slot guard compared an unrelocated SB instruction (a0450680).
That instruction references data+0x680 and its LO16 immediate changes when the
loader relocates the data segment. The guard can therefore reject valid loaded
code before any instrumentation is installed. This was a diagnostic defect;
it is not evidence that the original browser teardown itself was corrected.

The guard now derives this word from the registered data-segment address and
requires the owned two-segment layout. The VSH monitor logs BROWSER_EXIT_HOOK
status independently of the exit callback, including precise preflight failure
codes. Status1 means installed; -120 identifies the stage0 delay mismatch.
The regression rejects the old on-disk word at a relocated address and accepts
the corresponding loaded instruction.

The trace also covers full plugin shutdown separately from view close: stages
6..8 are calls at d9c4,d9cc,d9d4 to d834,d804,11fc. This includes the wait for
the shutdown event. Stage0..5 still cover the earlier view-close function.
The diagnostic now uses menu ABI5; install the two 0.24 modules together.
The actual launch/exit sequence remains unchanged pending a valid device trace.

Both PSP modules build with warnings as errors. The six XMB/VSH tests pass,
including relocated preflight, delay preservation, user bridge forwarding and
live-owner unload refusal. A standalone online close test using HOME then
Settings is required after installation. RSS can then provide a comparison for
the specific operation that stalls; copying mode12 alone is still unsupported.

## Disconnect lookup change (VSH entry/bridge 0.25)

The user provided two fully-loaded-page exit runs: Circle powered the ESP off
but left the UI unresponsive; HOME/XMB exit retained ESP power until shutdown.
Installed-file hashes and the trace marker identified 0.23, so neither run
contained the corrected 0.24 close trace. Logs are preserved in
artifacts/browser-exit-two-runs-20261119.

The latest completed Circle disconnect reached facade phase40 after helper
cleanup, consistent with rail power-off. The final XMB disconnect reached
phase21 (state publication) and stopped before callback validation/invocation
and cleanup. A user-side module-information lookup sits between these records.
This is consistent with a ModuleMgr lock cycle during module stop; without
thread stacks it is a candidate cause, not a proven explanation of both hangs.

0.25 replaces teardown/monitor owner queries and per-event callback validation
with guarded kernel exports using direct LoadCore UID lookups under an interrupt
guard. Only already-registered native consumers are accepted; recorded text
addresses, callback range/alignment and GP still have to match. Module identity
checks are retained. Handler reservation and removal revalidation remain, so
removal during validation cancels delivery before the callback runs.

Regression tests forbid user ModuleMgr lookups during event delivery, handler
removal and connection monitoring, including removal during validation and an
invalid owner. Kernel tests reject dead/replaced owners, wrong GP and callbacks
outside module text. Thirteen native/XMB/VSH tests pass under ASan/UBSan and both
PSP modules build with warnings as errors. 0.25 includes the corrected close
trace and its independent hook-status logging. No ESP firmware, launch mode,
certificate policy or power timing is changed. Hardware exit verification is
pending; Circle and XMB outcomes must still be tested separately.
