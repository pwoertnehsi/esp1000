ESP Browser Test v0.8 — PSP-E1000, 6.61 ARK-5

This app tests Sony's original browser with the preconfigured ESP Wi-Fi adapter.
It is not a system-wide auto-start plugin or full stock Wi-Fi setup support.
Use ESP firmware v11. Connect both ESP cables using the already-tested wiring
and enable the configured hotspot. No additional wiring changes are needed.

Copy BOTH PSP/ and b1/ from artifacts/browser-local/ to the memory-stick root.
Replace the three binaries in PSP/GAME/psp-browser/. Keep the existing b0/ folder:
its htmlviewer.res is still required. b1/ contains five original encrypted Sony
modules; v0.6 verifies their new sizes/CRCs before loading. The decrypted module
copies in b0/ are retained but unused. browser-test-code.zip excludes Sony files,
so that ZIP alone is insufficient for this update. Do not redistribute b0/b1.

1. Launch ESP Browser Test v0.8. CROSS verifies assets and connects the adapter.
   It checks native TCP both with explicit and automatic source addressing,
   then native DNS. Automatic-source TCP must pass before browser launch.
2. TRIANGLE opens the file page. Test Close Internet Browser first. HOME now
   requests Sony's normal close callback, then waits for QUIT before shutdown.
   Automatic Wi-Fi disconnect is disabled for this shared adapter session.
3. In a fresh launch, START opens http://192.168.77.1:8080/ on the ESP. It should
   display "PSP native TCP probe". This isolates the browser HTTP path from
   DNS, hotspot internet access and remote server behavior.
4. SQUARE opens http://suckless.org/faq/ in a fresh launch. Exit through the menu
   or HOME. CIRCLE exits the launcher once the viewer is closed.
5. Return ms0:/browser-test.log and ms0:/psp-vnet.log, noting which page/exit
   worked. BROWSER_CORE/BROWSER_WAIT record normal-close progress. TCP_TRACE
   records endpoints, flags, sequence numbers and checksum validity, no content.
   crc is IPv4/TCP: 1 valid, 0 invalid, 2 TCP not applicable. LOG_DROPPED reports
   bounded log queue overflow. Packet forwarding no longer writes/syncs the stick.

DNS is cancelled after eight seconds or on link loss/CIRCLE/HOME. If native
cancellation also stalls, buffers remain retained; DNS_RESOURCES_RETAINED is
logged and a restart may be needed. No running thread is force-killed.

Saved profile keys are never read or changed. The helper temporarily redirects
six APCTL entry points to a real-link-backed test session and redirects missing
browser module assets through brows0:/ and resources to b0/. Exact firmware guards and transactional restoration
protect these RAM changes. The read-only brows0: device exposes only the five
original module assets from b1/ and supports their native load-to-block checks; the
resource remains on ms0:. Global memory-stick loading rules are unchanged. No flash files, XMB icons or PSP model are changed.
The helper also bypasses four exact E1000 rejection branches inside the browser
wrapper. It preserves the actual PSP model and leaves all other modules alone.
PATH_STATUS should show gates=4. These RAM changes restore on exit.
The hook is scoped to this launcher, not ready for use in arbitrary applications.

CIRCLE/HOME cancels startup between operations. If a native call does not return
or shutdown cannot complete, resources are retained; a restart may be required.
No live thread is force-killed. Exit the app to disable; no seplugins entry is
needed. Remove PSP/GAME/psp-browser/ and its b0/b1 assets to uninstall.

v0.6 rendered the browser/file page, but Close Internet Browser stalled and
internet pages timed out. v0.7 passed clean close, file rendering and ESP HTTP on hardware. Internet
stalled when the ESP USB transfer watchdog reset the link. v0.8 separates USB
servicing from native RX injection and TX draining using bounded byte queues.
It preserves the native memory pool until all workers finish. PIPE diagnostics
show queue occupancy and native call stages. Internet recovery needs testing.
ESP firmware v11, b0/b1 assets and wiring are unchanged from v0.6.

For v0.8, copy updated PSP/ only. Start with CROSS then SQUARE (Suckless FAQ),
then try http://suckless.org/ from its home link or address bar. Return both logs.
Keep UART bridge running so ESP watchdog/router counters can be inspected.
PIPE RX stages: 0 idle,1 mbuf allocation,2 native enqueue. TX stages: 0 idle,
1 native dequeue,2 mbuf copy,3 mbuf free. A full ingress queue drops a packet
for TCP to retransmit; it does not wait or reset the USB host. Stop refuses
teardown if a native worker cannot finish within two seconds.
