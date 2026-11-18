# Upper Network Modules: 6.61 ARK-5

## Hardware Evidence

The user ran `net_probe` v1 on PSP-E1000 with 6.61 ARK-5. The supplied
`logs/net-probe.log` records devkit `06060110`, model `10`, and:

- COMMON load: `00000000`; ifhandle (`sceNet_Service`) and `sceNet_Library` appear.
- INET load: `00000000`; INET, APCTL and resolver appear.
- INET and COMMON unload: both `00000000`; launcher reaches its final END record.
- `sceWlan_Driver` was already resident before either utility group loaded.

This proves loadability on this console, not native initialization, successful
radio attach, working callbacks, or packet delivery. CFW version API returns
`2`/`0`; these are not an ARK-5 release identifier. CFW identity is user-confirmed.
Flash directory reads from the user-mode probe returned `8001000d`; the separate
`module_listings.txt` provides the actual listing. Do not interpret access denial
as missing files.

## Exact Library/NID Dependency Surface

The current inventory covers 34 supplied/recovered decrypted PRXs. `network_dependency_edges.json`
resolves function imports by library **and** NID: 1,729 of 3,886 imports have a
provider in this set. Other firmware libraries were not supplied; unmatched
imports are not automatically missing on the console.

| Consumer | Native lower-level imports resolved in the supplied set |
| --- | --- |
| pspnet | 19 ifhandle library functions; 1 WLAN library function |
| pspnet_inet | 18 ifhandle library functions; 64 net library functions |
| pspnet_apctl | 1 public WLAN + 2 WLAN library; 2 public net + 25 net library; 9 public INET + 16 INET library |
| pspnet_resolver | 2 ifhandle; 5 net library; 7 public INET + 1 INET library |

INET calls ifhandle directly, so preserving only the public `sceNet` facade is
insufficient. APCTL directly imports switch state plus WLAN device attach/detach.
Its Bluetooth-DUN and utility imports must be evaluated by flags/call paths rather
than treated as mandatory absent hardware. Loading succeeded with the real CFW.

## Interface Control ABI: New Call-Site Evidence

These are observations from the supplied, pre-relocation images, **not callable
prototypes**. Never use the offsets below directly as runtime patch addresses.

In WLAN device attach at `0x6c0`, the caller-owned interface record is embedded
at driver-state offset `0x188`. The code calls ifhandle create, supplies driver
context through another ifhandle import, installs callbacks at record offsets
`0x08`, `0x0c`, `0x10`, `0x14`, then attaches using a six-byte MAC and interface-name
pointer. The callback bodies in this image are `0x9fc`, `0xb08`, `0x3fb4`, `0xbe4`.
The attach wrapper `ifhandle:0xa34` supplies its own fourth argument (`1`).

The create helper `ifhandle:0xfe8` stores an allocated internal object in record
word zero and registers the **external record** in an eight-entry registry.
At ioctl dispatch `0x1cc`, the incoming first argument is compared with the value
in record word zero. The dispatcher then calls the callback at **external record
+ `0x14`**, passing that external record as its first argument and a local pair of
validated nested pointers as its fourth argument. Thus internal and external
handles are distinct. Swapping them would violate the observed call contract.
The numeric request selects nested-pointer validation via a jump table; its full
request/payload map and asynchronous completion ownership still need recovery.

Focused disassembly: `wlan_attach_callbacks.asm`, `ifhandle_ioctl_dispatch.asm`.
Do not replace this callback with an always-success stub: native WLAN's callback
queues command work and completes state changes through further ifhandle calls.

## Browser Assets Recovered

The console flash listing includes the browser wrappers and `libfont_hv.prx`,
but omits `htmlviewer_ui.prx` and five other browser files. Both supplied wrappers
import `sceHVUI_Module` (16 and 14 functions respectively). The provider and its
private dependencies are now recovered from the verified full 6.61 update;
all browser-specific function imports resolve within the expanded local set.

The update's 11g table omits the same six files. The console listing is consistent
with that table. This does not identify VSH's menu gate or establish a need for
flash writes. See `BROWSER_LOAD_FINDINGS.md` and `browser_asset_matrix.json` for
explicit load sites, source hashes and staged assets. Browser utility's actual
module name is `sceVshHVUtility_Module`; netconf's is `sceVshNetconf_Module`.

## Reproduction and Next Test

```sh
python3 tools/collect_sdk_nids.py /opt/pspdev/psp/sdk/lib/libpspnet*.a /opt/pspdev/psp/sdk/lib/libpspwlan*.a /opt/pspdev/psp/sdk/lib/libpsputility*.a --output analysis/network_sdk_nids.json
python3 tools/inspect_prx.py modules/*.prx --nid-map analysis/sdk_nids.json --output analysis/module_inventory.json
python3 tools/map_prx_dependencies.py analysis/module_inventory.json --output analysis/network_dependency_edges.json
make probe
```

The NID collector requires installed `pyelftools` and PSPSDK. SDK names annotate
symbols; they do not establish private prototypes. Inputs remain unchanged.
The v2 PSP probe adds an opt-in SQUARE native NET/INET initialization and binary
UDP loopback check after CROSS module loading. This baseline must be distinguished
from the subsequent virtual-interface ARP/ICMP and USB-routing milestones.

## Subsequent Gates Passed

The user ran v2 successfully: native NET/INET initialization, binary UDP loopback
and full cleanup passed. A verified full Sony 6.61 update supplied the missing
browser cluster; all browser-specific function imports now have local providers.
See `../milestones/native-sockets-and-browser-assets.md`. The missing-component
observations above describe the console's flash, not the now-expanded local set.
