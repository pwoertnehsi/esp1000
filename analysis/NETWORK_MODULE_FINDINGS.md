# Supplied Network Modules: Static Findings

Inspected on 2026-11-15. Inputs remain unmodified. This is a static inventory,
not a claim that these modules initialize on this E1000 or that private ABI
prototypes have been recovered.

## Reproduce

```sh
python3 tools/inspect_prx.py modules/*.prx \
  --nid-map analysis/sdk_nids.json --output analysis/module_inventory.json
```

`inspect_prx.py` uses only the Python standard library. `sdk_nids.json` contains
matching annotations extracted from the installed PSPSDK import archives;
unresolved NIDs are deliberately retained. The sectionless inputs use first
PT_LOAD `p_paddr & 0x7fffffff` as the **file offset** of SceModuleInfo. Library
pointers and function addresses are virtual addresses mapped through PT_LOAD.
Exports and import function trampolines are parsed; import-variable relocation
structures are not decoded. `local_module_edges.json` matches library + NID.

## Actual Boundaries

| Input | Module name | Exported function entries* | Imported function entries |
| --- | --- | ---: | ---: |
| `ifhandle.prx` | `sceNet_Service` | 104 | 27 |
| `wlan.prx` | `sceWlan_Driver` | 61 | 109 |

*Counts include functions exported through multiple libraries, not unique bodies.
These two modules export `sceNetIfhandle{,_lib,_driver}` and
`sceWlanDrv{,_lib,_driver}` respectively. WLAN imports 16 functions from
`sceNetIfhandle_driver`; all 16 resolve to the supplied ifhandle image.

| Candidate ifhandle operation | Export NID | Image-relative VA |
| --- | --- | --- |
| `sceNetCreateIfhandleEther` | `16042084` | `0x0968` |
| `sceNetDestroyIfhandleEther` | `C9344A59` | `0x0984` |
| `sceNetAttachIfhandleEther` | `AE81C0CB` | `0x0A34` |
| `sceNetDetachIfhandleEther` | `54D1AEA1` | `0x0A78` |
| `sceNetIfDequeue` | `E2F4F1C9` | `0x03B4` |
| `sceNetIfEnqueue` | `C28F6FF2` | `0x0AC0` |
| `sceNetIfPrepend` | `16246B99` | `0x0AF8` |
| `sceNetMallocInternal` | `15CFE3C0` | `0x18B4` |
| `sceNetFreeInternal` | `76BAD213` | `0x193C` |
| `sceNetMFreem` | `F56FAC82` | `0x1B18` |
| `sceNetIfhandleIfIoctl` | `0296C7D6` | `0x01CC` |

Names are cross-referenced against the [PSP NID database](https://github.com/pspdev/psp-ghidra-scripts/blob/master/ppsspp_niddb.xml)
and [PSPLibDoc's ifhandle table](https://pspdev.github.io/psplibdoc/modules/ifhandle.prx_sceNetIfhandle_driver.html).
The PSP name-hash algorithm independently reproduces these NIDs;
`ifhandle_names.json` records that mapping. Names alone do not establish
arguments, ownership, dispatch direction, or concurrency rules.

## Narrow Reverse Dependency

ifhandle imports exactly these three functions from `sceWlanDrv_driver`:

| WLAN NID | WLAN VA | Static evidence |
| --- | --- | --- |
| `2519EAA7` | `0x17664` | Drop-decision candidate: tests rate state, timer and PRNG; ifhandle dequeue calls it and frees a packet when result is positive. |
| `325F7172` | `0x17828` | Drop-rate setter: called by ifhandle's `sceNetSetDropRate` wrapper at `0x0D8C`; checks first argument is at most 100 and stores two settings. |
| `B6A9700D` | `0x177EC` | Drop-rate getter: called by `sceNetGetDropRate` wrapper at `0x0D24`; validates two pointers and writes stored settings. |

This supports separating interface services from radio hardware. It does not
prove that replacing these three imports is sufficient for all network modules.
The supplied WLAN module itself imports Sysreg, Syscon, DMA, power, IDStorage,
and other hardware services; calling through to native attachment is not an
established way to emulate a radio.

## Structure Evidence and Limits

Focused disassembly is saved as `ifhandle_*.asm` and `wlan_drop_helpers.asm`.

- The constructor wrapper at `0x0968` calls `0x0FE8` with size `0x140`.
  The latter allocates an internal object, stores its pointer in the caller's
  first word, initializes semaphores and an eight-slot handle registry.
- Attach helper `0x111C` clears queue fields at handle offsets `0x18`, `0x1C`,
  `0x20`, `0x24`, `0x28`, and copies a six-byte MAC into internal storage.
- Dequeue at `0x03B4` reads the handle's queue head at `+0x18`, follows a packet
  link at `+4`, updates the tail/count, and has callback/drop/free paths.
- These observations are anchors for ABI recovery, **not** a complete C header.
  Do not equate an emulator's handle size with the internal allocation size or
  assume every queued message is a flat Ethernet frame.

## Fingerprints and Missing Upper Layers

`ifhandle.prx` SHA-256:
`02cb347eb375f7c9a0040d7ab0e137ba60cf96a0c4cf995556aa1a6c23dcf80e`

`wlan.prx` SHA-256:
`d00fcfe3348e030384269bbf149285ae643574678b16900265273f476f2807ef`

All six supplied files are decrypted little-endian MIPS ELF PRXs with program
headers and no section table. Export addresses above are **pre-relocation image
offsets**, not writable runtime addresses. Firmware identity must be checked on
the PSP; module version bytes are not the console firmware version.

The directory does **not** contain `pspnet.prx`, `pspnet_inet.prx`,
`pspnet_apctl.prx`, `pspnet_resolver.prx`, or the adhoc modules. Their imports,
ioctls, model gates, and event sequences remain unverified. Additional decrypted
copies or runtime import traces are an explicit first implementation gate.
