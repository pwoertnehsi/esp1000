PSP Virtual Interface Test v0.5
Target: PSP-E1000, 6.61 ARK-5

Replace EBOOT.PBP and psp_vnet.prx together in /PSP/GAME/psp-vnet/.
This pair has a new startup identifier and refuses mixed older files.
No ESP update or wiring changes are needed. Disable RemoteJoyLite.

1. Launch "PSP Virtual Interface Test". Check the v0.5 startup line.
2. Press CROSS to register the private interface (result 0, flags 7).
3. Press SQUARE once for the native ARP/eight-datagram UDP test.
4. Press CIRCLE or HOME to cancel/clean up and return to XMB.
5. Copy the complete /psp-vnet.log into the project's logs/ directory.

Network work and cleanup now run on job threads. The main thread continues
handling CIRCLE and HOME. A network phase stalled for five seconds logs the
thread/wait state and requests cancellation through Sony's thread-abort API.
If a native call cannot unwind, the app reports it and retains live resources;
a console restart is still required in that case. No thread is force-killed
while it may own native locks or buffers.

Configuration no longer uses the combined delete-and-add wrapper where v0.2
stalled. The fresh interface receives an add-only 64-byte IPv4 alias request,
with separate logs for socket creation, address assignment, and socket close.
The packet worker never calls native dequeue on an empty TX queue: that call
signals the native event loop even when it returns no packet.

Success says "Local packet lab passed". This is a gateway fixture inside the
PSP driver, not USB or ESP Wi-Fi forwarding. It uses 192.168.77.2/24 and tests
Ethernet sizes 64/65/208/209/512/1500/1514 with binary payloads. No flash files
or saved network profiles are changed.

v0.1 registration/callback dispatch and cleanup passed on PSP. v0.2 stalled
inside combined address configuration. v0.5 hardware validation is pending.

v0.3 hardware diagnostics identified a missing completion for address ioctl 2:
the job waited on the exact completion semaphore logged by the driver. v0.5
acknowledges address ioctls 2/3 through the native completion export, once each.
The native abort API did not release the v0.3 semaphore wait; do not interpret
its zero result as successful cancellation. v0.4 passed all eight UDP cases and full cleanup on PSP. The default build
retains this local fixture; make usb-virtual selects a separate USB/ESP backend.
See USB_README.txt for that v0.5 hardware test.
