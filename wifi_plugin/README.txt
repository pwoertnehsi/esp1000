USB diagnostic candidate v0.18 / ESP17: logs enumeration stages, frame-header
validation reasons and endpoint pending/completion progress. No payloads are
logged. Retry rules, timeouts and browser lifecycle are unchanged. The PSP USB
transport log adds USB_DIAG; ESP status adds ENUM_DIAG and BULK_DIAG.

USB diagnostic update v0.18 + ESP17:
Enumeration progress, bounded frame validation reasons and endpoint pending/
completion times are recorded; no payloads are logged. Timeouts and recovery
policy are unchanged. psp_vnet.prx and wifi_native_usb.prx share the updated
transport and must be installed with this package. Test connected browser idle
for60 seconds, then one HTTP page; capture UART and copy logs after exit.
This is diagnostic work, not a demonstrated dropout fix.

ESP WiFi native browser candidate v0.18 — E1000 / owned 6.61 / ARK-5 / ESP17

Browser and Online Manual now use Sony's original VSH actions. L+R+SQUARE queues
that same native browser action. The plugin does not load an EBOOT, draw a GU
screen, or call ExitGame for these entries. Sony owns browser rendering, exit
and its existing restriction/password flow; that flow is not bypassed or replaced.
Native browser launch and exit passed on hardware with v0.15. The four-digit
prompt remains untested. v0.16 also routes Sony HTTP GetInfo to the ESP session;
this fixes the missed consumer implicated in instant 80410A05 on remote pages.
Remote browsing passed on hardware with v0.16. v0.17 corrects connection-status
security info: AES is3 for APCTL GetInfo, distinct from AES4 in profiles/scan.
This prevents the browser indexing past its four-entry security-label table.
Display connection status still needs a hardware retry.
Display Connection Status: v0.17 reports AES as APCTL value3 (profile/scan
value4 is a different enum), preventing an out-of-range Sony label-table index.
Hardware verification of this fix remains pending. WPA2-Personal is now user-
confirmed on a real router without an ARK WPA2 extension.
The standalone browser EBOOT is retained as a separate diagnostic fallback.

Install wifi_vsh.prx and wifi_menu.prx together (shared control ABI changed).
Keep the existing native helpers, virtual driver and private ms0:/b0 + b1 assets.
The connection-status fix also works with ESP15. ESP16 adds bounded known-PSP
enumeration recovery (15 seconds, up to three attempts); this does not establish
the cause of every failure requiring a reset. After leaving PC USB mode, restart VSH.
Test native XMB Browser, exit to the same XMB, reopen, then test the shortcut.
Read wifi-vsh.log for NATIVE_BROWSER path/backend status if startup fails.
Network Settings Scan, Test Connection and saved-profile browser use passed
with v0.14 / ESP15; repeat these checks with this new candidate.

Native Infrastructure scan and connection requests now have an ESP backend.
Native Scan showed real access points and Test Connection passed on hardware
(user confirmed 2026-11-18). Repeatability and coexistence checks remain pending.
Keep your original Sony assets.
Plugin entries remain:
vsh, ms0:/seplugins/esp-wifi/wifi_vsh.prx, on
vsh, ms0:/seplugins/esp-wifi/wifi_menu.prx, on
The two extra native_usb/native_session PRXs are loaded on demand; DO NOT add
extra plugin entries for them. Install the complete candidate together with ESP15.

ESP15 boots an unconfigured station. No SSID/password is compiled into firmware
and no private credential header is used. Sony's saved/temporary PSP profile is
the source of SSID, security, key, DHCP/static IP and automatic/manual DNS.
Only a complete validated USB transaction applies the profile. ESP configuration
stays in RAM; the PSP registry owns saved profiles. Sensitive transfer buffers
are cleared and no keys or SSIDs are added to diagnostic logs.

After safe PC eject and VSH restart, connect the externally powered ESP.
Settings > Network Settings > Infrastructure > New Connection > Scan should show
real2.4GHz results in Sony's own list. Hidden SSIDs can be entered manually.
Select the network, enter its key using the native UI, save, and test connection.
WEP40/104, WPA-TKIP/AES passphrases/raw PSKs are translated from Sony's format;
WPA3-only/enterprise networks remain marked unsupported by the native chooser.
WPA2/WPA3 transition networks can use their WPA2-compatible AES mode.

Native status/state/events follow actual ESP association and a PSP virtual
Ethernet/local TCP/DNS readiness check. The on-demand session borrows Sony NET
when already initialized and tears down only its own resources. USB is released
after a scan, a failed request or Disconnect. Busy/unsafe ownership is refused.
Test ordinary USB file transfer/camera after leaving Settings. Do not assume
coexistence is hardware-proven until that test passes.

Diagnostic files: wifi-vsh.log (NATIVE_BACKEND status1 means hooks installed,2 means rebound after a weak-library change;
-14/detail identifies an import guard refusal; -22 is weak-import metadata mismatch), native-wifi.log (result/phase,
no credentials), psp-vnet.log, browser-test.log. Native backend phases:
1 NET,2 INET,3 resolver,4 virtual driver,5 link,6 local TCP/route,7 DNS config,
8 DNS query,9 adapter,10 ready. A native Sony connection test may still fail
against retired Sony service endpoints; it is not a simulated success.

Ad Hoc remains distinct, with no ESP ad hoc/Game Sharing backend implemented.
Other restored legacy service icons do not imply those services work.
Network and PSN categories remain visible. No global model/switch spoof or flash
writes. Exact module, import target, callback and code guards refuse mismatches.

Browser remains the separate proven launcher (v0.12), through XMB Browser or
L+R+SQUARE. It uses the ESP association established by native Settings; after an
ESP power cycle, connect through Settings again. Browser still has a black
LoadExec transition and recreates VSH on exit. A seamless resident browser and
native profile picker at browser startup remain unfinished work.

Disable both added plugin entries and restart VSH for recovery. Source and prior
ESP13/browser/plugin rollback archives remain in the project. This candidate
contains only our code; Sony modules/resources remain personal on the PSP.

V0.11 accepts the exact loader syscall21/NOP state of unresolved weak APCTL
imports after checking original NID and library metadata. Later APCTL load and
owned NET module unload explicitly refresh only validated owned/unlinked/native
import slots. Unknown or foreign jumps remain rejected atomically. Extra stub/API
words in wifi-vsh.log distinguish missing providers from unexpected instructions.
No ESP firmware update from14 is required.

V0.12 uses the verified kubridge owner lookup for native USB helper startup,
preserves loader/StartModule failure codes, and records startup phases101-105,
USB worker phases1001-1008 and native profile parameter reads200+parameter.
USB controlABI4 requires updating native_usb and the user bridge together.
ESP14 wire protocol is unchanged; no firmware flash is required.

V0.13 borrows an already-started inactive USBBusDriver in VSH. It does not start
that shared bus twice or stop it during cleanup. An activated bus, foreign
storage/camera/GPS/communication driver, or existing accessory owner is refused.
A race returning80243001 is accepted only for the base bus with its started
state rechecked and no foreign owner. ALREADY from other startup steps is still
a failure. USB failure phases1041..1046 identify register/type/bus/accessory/
custom-driver/activate respectively. Both native_usb and psp_vnet contain this
shared transport update. ESP14 and browser EBOOT unchanged.

V0.14 is a diagnostic correction, not a claimed scan fix: native scan errors
are preserved end to end instead of becoming-1. Phase1007 detail is the exact
scan error:1 link/not-ready,2 busy,3 ESP scan failure,4 invalid/incomplete reply,
5 timeout. ESP15 logs Scan requested/completed and raw SDK status/count without
SSID/BSSID/keys, and UART status includes the latest scan result. Keep ESP powered
and capture UART during Scan so a later board reset does not erase the evidence.
Failed helper cleanup retains the service and refuses reuse until safe cleanup.
The wire protocol and USB controlABI4 remain unchanged.
