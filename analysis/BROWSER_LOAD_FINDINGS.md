# Browser Load Path: Supplied 6.61 E1000 Firmware

The user supplied decrypted `utility.prx`, `vshmain.prx`, and a complete
`flash0:/vsh/resource/` listing after the first network probe.
Fingerprints and library inventories are in `browser_load_modules.json`.
These are pre-relocation offsets, not runtime patch addresses.

## Recovery Status

The verified full Sony 6.61 update is now extracted locally. All six missing
browser files were recovered; see `browser_asset_matrix.json` and
`../milestones/native-sockets-and-browser-assets.md`. The update's 11g table
omits them, agreeing with the user's installed E1000 listing. Runtime browser
restoration and the VSH menu gate are still untested.

## Confirmed Dependencies and Paths

Both browser wrappers import `sceHVUI_Module`. The utility driver's helper at
`0x236c` selects explicit firmware module paths by module-name string. Its
`htmlviewer_ui` case references `flash0:/vsh/module/htmlviewer_ui.prx` through the
instruction at `0x2420`; its `netfront` case references
`flash0:/vsh/module/netfront.prx` through `0x24ac`. Both converge on the load helper
at `0x28f4` with the selected path.

The load helper calls `ModuleMgrForKernel` NID `D4EE2D26` at stub `0x8808` when a
memory-block argument is supplied. The installed 6.60 kernel SDK annotates this
NID as `sceKernelLoadModuleToBlock`. A second load branch uses NID `939E4270` at
`0x8800`; do not assign an unverified prototype to it. This means a compatibility
loader must preserve the requested memory-partition/block behavior, not merely
load the PRX arbitrarily and report success.

The user's flash listing contains neither `htmlviewer_ui.prx` nor `netfront.prx`.
It does contain `htmlviewer_plugin.prx`, `htmlviewer_utility.prx`,
`libfont_hv.prx`, `htmlviewer_plugin.rco` and `netconf_dialog.rco`.
The resource listing does not contain `htmlviewer.res`, a filename referenced by
both supplied browser wrappers. Its precise role and requirements need further
analysis; a string alone does not establish when it is required.

## Implications for a Memory-Stick Installation

The missing viewer library and engine are more concrete blockers than the menu
icon alone. Request matching files from a full 6.61 update dump, rather than ask
the user to search the same E1000 flash directory again.

The explicit load sites provide a candidate for **scoped RAM path redirection**
to memory-stick files. This is a feasibility lead, not tested restoration: original
module imports, resource access, memory-block allocation, model gates and teardown
must all work. Keep any future redirection restricted to the known browser load
sites, verified fingerprints and exact paths. Avoid global I/O redirection or
model impersonation. Do not change flash contents.

`vshmain.prx` is `vsh_module` and contains the original browser launch strings,
plugin paths and `msgtop_network_browser` text identifier. Their presence does not
prove the E1000's menu branch enables the entry. An original-viewer homebrew launch
and restoration of its stock XMB item remain separate acceptance targets.

The needed files and their additional private libraries are now recovered.
Further stock firmware dependencies can be inspected in the local full extraction. No evidence yet requires flash writes;
the user must decide if later investigation establishes such a requirement.
