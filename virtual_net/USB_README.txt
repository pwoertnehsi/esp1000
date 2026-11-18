PSP Native Internet Test v0.8 / ESP firmware v10
PSP-E1000, 6.61 ARK-5. Memory-stick installation; no flash/profile writes.

Replace BOTH EBOOT.PBP and psp_vnet.prx in PSP/GAME/psp-vnet-usb.
Keep the established wiring and RemoteJoyLite disabled.

1. Launch v0.8. CROSS registers the interface. Wait for USB=1.
2. Optional SQUARE repeats the native ARP/eight-UDP regression.
3. TRIANGLE runs a genuine native TCP/HTTP check to 192.168.77.1:8080.
   This uses the ESP's local TCP listener; Wi-Fi/NAT/default route are bypassed.
   Success says Local TCP passed. This probe does not create a file.
4. After local TCP succeeds, START is enabled for the internet test. Enable the
   hotspot and wait USB=1 Wi-Fi=1 target=1. START adds the RAM-only default route,
   requests http://neverssl.com/ using PSP TCP, then saves validated HTML body
   to ms0:/native-http.html. Each test runs once per launch.
5. CIRCLE/HOME cancels/cleans up. Copy ms0:/psp-vnet.log to logs/.

v0.7 accepted the default route and sent TCP SYNs, then the console crashed
while connecting. v0.8 fixes a concrete RX ABI mismatch: normal Ethernet frames
start at mbuf+50, matching Sony WLAN's two-byte alignment padding. Removing the
14-byte Ethernet header now leaves IPv4/TCP word-aligned. First-node capacity
is206 bytes; subsequent nodes hold236. RX logs include alignment and TCP flags.
Whether this fully fixes the crash requires the user's PSP run.

The ESP v8 accessory settle/retry/serialization fix remains. v9 routing/NAPT
remains, and v10 adds the local TCP probe bound only to the private USB LAN.
The ESP resolves the fixed internet hostname; the PSP originates TCP and HTTP.
The older HTTPS receiver still activates only for its own application.

Network jobs run separately from controls with deadlines and cancellation.
A permanent native lock may still require restart; no live native thread is
force-killed. File publishing preserves the previous completed .html via .bak
and rejects truncated/compressed/chunked/oversized responses. Local TCP is
required before internet testing. APCTL/profiles/DHCP/browser remain future work.
