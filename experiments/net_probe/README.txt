PSP Wi-Fi Compatibility Check v2

Copy this net-probe folder into /PSP/GAME/ on your memory stick.
Exit PSP ESP Link before launching this app. Disable RemoteJoyLite.
The ESP does not need reflashing and the wiring can stay as it is.

1. Launch "PSP Wi-Fi Compatibility Check" from Game > Memory Stick.
2. Wait for "Inventory saved". This checks module metadata and lists
   firmware PRX files, without changing flash files or network profiles.
3. Press CROSS once to try loading stock COMMON and INET modules.
   This executes Sony startup code, which may fail or hang on an E1000.
   Each attempt is logged and synced first. If it hangs, restart the PSP.
4. After INET succeeds, press SQUARE once for the native UDP loopback test.
   This initializes Sony's network/socket libraries and exchanges a binary
   datagram on 127.0.0.1. It does not connect to an access point. The receive
   wait is limited to one second; Sony init/term calls may still fail or hang.
5. Otherwise press CIRCLE or HOME to return to XMB.
6. Copy /net-probe.log from the memory stick into the project's logs/ folder.
   Logs append across runs; keep the full file, including failed attempts.

CIRCLE/HOME without CROSS performs inventory only. Module loading is skipped
if logging fails. Only modules successfully loaded by this run are unloaded,
in reverse dependency order. A failed INET unload prevents COMMON unloading.

This is a diagnostic, not a working stock WLAN driver or browser restoration.
Kernel table addresses are recorded, never dereferenced or patched.
Directory access errors do not prove that the firmware files are absent.

A passed loopback is a local PSP stack baseline, not USB/Wi-Fi networking.
This project targets PSP-E1000 on 6.61 ARK-5. WPA2 compatibility is supplied
by ARK-5; no separate WPA2 plugin is needed for this project target.
If native library cleanup fails, the app retains backing modules until exit.
