# ESP1000 Ad Hoc over a local network

This candidate adapts [Kethen/aemu](https://github.com/Kethen/aemu), pinned at
`f6bc457eca56c4f08cad39aeac80fcc6dce6db6a`, to the ESP-backed E1000.
It is a GAME plugin, separate from the VSH/browser plugin. It has not yet passed
hardware peer exchange or a Gran Turismo race. Ordinary PSP radio Ad Hoc, Game
Sharing and public relay servers are not implemented by this candidate.

The existing aemu modules supply Ad Hoc control, discovery, matching, PDP, PTP
and GameMode implementations. Our shim reads the PSP's saved infrastructure
profile, applies it through the existing native USB helper, starts the virtual
interface and configures its address/default route. No SSID/password is compiled
in. Only this opt-in GAME plugin's user imports are redirected to the ESP shim;
Sony providers and VSH are not patched by the new transport. The shim is excluded
from its own import redirection. Existing clients are rebound when the shim or
APCTL provider loads, to handle late weak-import resolution. The software WLAN
switch means this transport is selected; connection state reaches GOT_IP only
when the real USB/uplink/metadata flags are present.

PPSSPP's [PRO-compatible server protocol](https://www.ppsspp.org/docs/development/adhoc/adhoc-server/)
uses TCP27312 for room discovery. Game traffic goes directly between peers.
The PSP is behind ESP NAPT, so aemu's port-open/close operations request explicit
TCP/UDP mappings at the USB gateway. ESP accepts this control protocol only from
its configured PSP address, correlates replies by nonce, tracks up to16 mappings,
and removes them on close, clear or USB link-down. It maps the exact offset game
port to the same private PSP port; it does not forward router WAN ports.

The current virtual interface has one fixed locally administered MAC. This
candidate targets one ESP-backed PSP and one PPSSPP instance at different LAN
addresses. Multiple ESP-backed PSPs need unique identities before this is usable
for them. The existing USB dropouts remain a limitation. A successful build or
room join does not establish reliable gameplay.

## Build

Clone the pinned upstream including submodules into `third_party/aemu`; that
checkout remains unmodified and ignored. With PSPSDK installed:

```sh
python3 tools/prepare_aemu.py --build
make -C adhoc/peer_test
python3 tools/package_adhoc.py --build artifacts/aemu-esp --server 192.0.2.10
```

Use a fresh `--output` for subsequent preparations/packages. The preparer keeps
upstream's GPL notices, uses its bundled CFW SDK stubs without installing them
into the system SDK, and selects GNU99 for compatibility with its older C.
Generated code stays under `artifacts/`. The package includes GPL licensing;
a separate source zip contains project source and the pinned upstream checkout.
Aemu's original automatic HOME/exit-callback suppression and power-button lock
are disabled for the E1000 adaptation. Other upstream game/memory patches remain.

Build ESP firmware after activating ESP-IDF, using the project's normal ESP
build procedure. The port-mapping addition is ESP firmware v20; older firmware
cannot acknowledge mapping requests. Do not mix this with an arbitrary atpro
release: use the adapted modules as one set.

## First peer test

1. On PPSSPP, enable networking/WLAN and its built-in Ad Hoc server. Set the
   server to the PC's LAN IPv4 address, port offset10000, direct/P2P mode. Disable
   relay, ENet server connection and UPnP for this local test. Permit TCP27312
   and the test's UDP13090 on the PC's LAN firewall. ESP and PC must share a LAN
   without wireless client isolation. Avoid changing the MAC to the PSP's MAC.
2. Run the packaged `ppsspp/esp-adhoc-peer/EBOOT.PBP` in PPSSPP.
   The identical PSP copy stays under `PSP/GAME/esp-adhoc-peer/EBOOT.PBP`;
   do not open that packaged path in PPSSPP, which interprets `PSP/GAME` as
   belonging to its own emulated memory stick. On the PSP,
   enable `game, ms0:/seplugins/esp-adhoc/atpro.prx, on` in ARK's plugin list,
   then launch **ESP1000 Ad Hoc Peer Test**. The VSH entries remain unchanged.
3. Press CROSS in both tests. They use product ID `ESP100001`, group `ESP1000`
   and PDP port3090 (offset to13090). Both must show received peer packets;
   TX alone or "group joined" is not a pass. Use HOME to exit.
4. Only after bidirectional traffic works, try Gran Turismo on both devices,
   using the same game region/revision and multiplayer room. Match the offset
   and server settings. Record lobby join, start, gameplay and exit separately.

Configuration lives under `ms0:/seplugins/esp-adhoc/`: `server.txt`,
`port_offset.txt`, optional `hotspot.txt`, and `nickname.txt`. With an empty
hotspot file, aemu uses the first existing saved profile. Credentials remain in
the native PSP profile. No firmware reinstall or flash0 writes are needed.

The peer-test startup stages are saved to `ms0:/esp-adhoc-peer.log`, including
failures before the ESP backend starts. PSP transport diagnostics go to
`ms0:/esp-adhoc.log` and `ms0:/psp-vnet.log`.
The ESP logs mapping results as `adhoc-map`. Keep build manifests and raw hardware
logs locally; they can contain user-specific identifiers. To disable this
candidate, turn off only its GAME plugin entry. Preserve the browser entries.

## First hardware startup failure

The peer-test log showed COMMON load succeeded but Ad Hoc module load returned
80010802, before NET initialization or ESP backend use. The adaptation now hooks
utility load/unload imports explicitly for E1000 user modules, including
high-address homebrew, rather than relying only on upstream's address heuristic.
A test executes the generated hook at a high user address and verifies routing
to the replacement loader, excluding foreign models and kernel modules.
`esp-adhoc-plugin.log` records startup/context and hook results independently of
upstream release logging. This candidate still needs a hardware retry.
