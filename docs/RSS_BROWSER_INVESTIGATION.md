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
build/browser-exit-two-runs-20261119.

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

## Synchronous diagnostic I/O removal (VSH entry/bridge 0.26)

The 0.25 XMB-exit report still failed. Hook status1 was logged; full HTML plugin
shutdown stages6,7,8 each entered and returned. No view-close stages0..5 were
recorded in that run. The façade again ended at disconnect state-publication
phase21, before callback validation/invocation or transport cleanup. The durable
line does not prove trace_event returned: its synchronous file close could also
be the blocking operation. 0.25 therefore did not establish a lifecycle fix.

0.26 removes synchronous memory-stick and USB-state operations from façade,
exit-trace and TLS diagnostic producers. A bounded 64-record queue is drained
by the resident browser worker, independently of disconnect delivery. Producers
use a non-waiting try-lock and drop records if busy/full. TLS diagnostics leave
16 queue slots available for other events. Only the drain checks mass-storage
ownership and opens the three fixed log paths. Dropped records are counted in
NATIVE_LOG_DROPPED records; no payloads or credentials are added to logs.

Façade semaphore waits now issue bounded diagnostic waits, recording failures
without a tight spin. Decimal phases50/51/52 (hex32/33/34 in the log) record the
waiting source line/error, the last acquiring source line/thread, and the lock
UID. These diagnose a remaining stall; they do not force-release a live lock.

Sixteen native/XMB/VSH/TLS tests pass under ASan/UBSan. They exercise producer
I/O exclusion, queue overflow/contention, destination routing, storage refusal
and lock-wait recovery in addition to existing ownership/lifecycle checks. Both
PSP modules build with warnings as errors. No ESP update, launch-mode change or
verified browser-exit success is claimed. Hardware XMB-exit verification remains
pending. Previous images and logs are archived in build/browser-exit-v026.

## Independent thread observation (VSH entry 0.27, bridge 0.26)

The 0.26 hardware run still hung. Sony shutdown stages6..8 returned again;
facade disconnect stopped after phase21. No semaphore-timeout or dropped-log
record was found. These records do not establish whether the facade worker was
blocked, suspended, terminated, or whether its log consumer stopped. Failure logs
are preserved in build/browser-exit-v026-failure. APCTL GetInfo was also
reviewed: its adapter already copies only the requested field's width.

0.27 adds an independent kernel-monitor snapshot to ms0:/browser-thread.log.
Slot0 is ESPNativeSettings; slot1 is ESPStockBrowser. Only entries inside the live ESP WiFi XMB
Bridge module's text are accepted. Records contain timestamp, UID, status,
wait type/object and exit status; they contain no stack contents or credentials.
Running and ready are normalized to status3 to reduce sampling noise. Raw other
status values are retained. A zero UID means no matching worker was enumerated;
truncated enumerations are ignored. Sampling occurs at most twice per second,
with a five-second heartbeat while the native worker exists. Transitions are
written by the kernel monitor, independently of the user-side log queue.
Storage ownership prevents file writes. No worker is resumed or forcibly stopped.

The kernel build passes warnings as errors and 17 targeted tests pass, including
storage refusal, module ownership, truncated enumeration, disappearance,
suspension, running/ready normalization and heartbeat. Only wifi_vsh.prx changes;
bridge0.26, ESP firmware, browser launch and network/power policy remain as before.
A hardware exit run is required. This is diagnostic instrumentation, not a
verified browser-exit fix.

## Circle result and separate Sony observer (VSH entry 0.28)

The requested 0.27 run was Circle exit, not HOME/XMB exit. These are distinct
failures: the user previously reported rail power-off with Circle, versus power
retained with HOME/XMB. Do not use a successful Circle transport teardown to rule
out the earlier HOME/XMB disconnect stall.

The latest Circle sequence reaches facade phase40 after helper/transport cleanup
and returns zero. The plugin workers appear alive and normally waiting in the
thread snapshots. Sony close-trace records were not appended for this Circle
run; the stage6..8 lines remain from earlier runs. The browser still hangs after
Circle. The observations narrow this path only; they do not resolve either exit.
Failure logs are archived under build/browser-exit-v027-failure-*.

0.28 moves observation to a separate kernel thread, ESPBrowserWatch, so it no
longer depends on progress through the main VSH monitor. It also samples threads
whose entry addresses belong to the live HTML plugin, HVUI, Netfront, HTTP, VSH,
PAF, launcher or RSS browser modules. browser-sony-thread.log records a timestamped
snapshot every two seconds while the HTML plugin exists and one final snapshot
after disappearance. Each row names the owner index in that order, thread name,
UID, module-relative entry, status and wait object. No stacks, page contents or
credentials are read. Existing plugin-worker observations remain in
browser-thread.log. Both files respect USB mass-storage ownership.

17 targeted tests pass and the PSP kernel build passes warnings as errors.
Watcher startup failure and timed-out shutdown retain safe ownership behavior.
This is further diagnosis, with no browser lifecycle or ESP change. Next test is
Circle again to capture Sony waits on that path; HOME/XMB needs its own separate
run, and RSS Guide can supply a successful-close comparison.

## Route HVUI's private APCTL subscriptions (VSH entry 0.29)

The 0.28 Circle capture identifies a persistent SceHtmlViewerPrepareDisconnect
thread, entry htmlviewer_plugin+0xbdec, sleeping while helper cleanup has already
returned successfully. Circle is distinct from the HOME/XMB failure; no inference
that HOME/XMB cleanup succeeds is made. Logs are preserved under
build/browser-exit-v028-failure-*.

Owned 6.61 firmware establishes a missing subscription path:

- PrepareDisconnect calls htmlviewer_plugin+0xbf80 with (0,0), which dispatches
  through the HVUI export 0x51684f2f (HVUI+0x1c64).
- That routine requests APCTL disconnect through HVUI+0x4e08 and, on success,
  waits at 0x1db4 for bit2 of its context+0x1c14 (while handler ID+0x1c08 >=0).
- The APCTL callback HVUI+0x2c20 sets that bit at 0x2ca8 when nextState==0 and
  event==5. Registration at HVUI+0x3154 uses private NID 0xa7bb73df, deletion
  at +0x316c uses private NID 0x6f5d2981.
- pspnet_apctl exports these at +0xe2c/+0xeac. The registration forwards the
  function and argument to its ordinary shared registration implementation
  (+0x2718), selecting handler class2. Its returned ID is stored by HVUI.
  The delete export selects the matching class2 pool. The callback is the same
  five-argument APCTL event ABI, not a descriptor or a different event format.
- Our HVUI guard covered GetInfo, Disconnect and GetState but omitted those two
  subscription imports. Thus Disconnect targeted ESP while the UI acknowledgment
  subscription remained on the real PSP WLAN backend.

0.29 adds the two exact owned imports to the existing atomic preflight/patch set,
using the facade's existing validated AddHandler/DelHandler callbacks. No shared
ABI, callback lifetime policy, launch mode or power sequence changes. Facade IDs
are opaque to this HVUI path; registration/deletion remain paired in its own
backend. No live callback is force-released and no UI completion flag is forged.
This directly addresses a demonstrated backend mismatch and the wait seen on the
Circle path. It does not establish hardware success or resolve the separate
HOME/XMB symptom yet.

Tests assert both private imports are patched, include the final private-delete
stub in foreign-stub atomic-refusal coverage, and exercise a UI-style disconnect
acknowledgment after deletion of a separate temporary handler. All 17 targeted
native/XMB/VSH/TLS tests pass under ASan/UBSan; kernel compilation passes warnings
as errors. Only the kernel PRX needs replacement, with bridge0.26 retained.

## Hardware result — 0.29

On 2026-11-19 the user reported: “both xmb and circle exits worked.” This confirms
successful hardware exit for both previously distinct failure paths after commit
8619c95. The private APCTL subscription routing fix is the successful candidate;
earlier logging/lookup changes did not independently resolve the issue. See
BROWSER_EXIT_MILESTONE.md for the tested configuration and validation limits.
