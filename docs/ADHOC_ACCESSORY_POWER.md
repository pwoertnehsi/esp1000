# E1000 Ad Hoc accessory power candidate

The GAME plugin requests the USB accessory side rail when the ESP APCTL backend
first connects. It does not enable power merely because a game starts, and makes
no VSH/browser power changes. It waits two seconds before starting either USB
owner, retains the lease across profile-helper / virtual-driver handoff and
Disconnect/retry, and releases after transport shutdown on APCTL Term. Module
stop and reboot-before provide game-exit cleanup.

The user verified that powering the ESP from the side rail before attaching USB
works. The earlier voltage sag alone did not demonstrate a hardware current
limit. `accpower` starts Sony camera drivers and activates PID 0x282; its app has
no explicit one-second cutoff. The camera/accessory stack's shutdown behavior
must not be reproduced as a recurring rail pulse.

The implementation calls `sceSysconCtrlUsbPower` directly, without loading camera
function drivers. Its command and payload match the owned 6.61 USB module and
[uOFW's Syscon implementation](https://github.com/uofw/uofw/blob/master/src/syscon/syscon.c).
The original NID 0x3987FEA3 uses the CFW import resolver; the 6.60 mapping is
0x47378317 in [uOFW's NID table](https://github.com/uofw/uofwinst/blob/master/SystemControl/nid_660_data.c).

During the lease, a RAM-only hook in the owned Sony USB module's
`sceSysconCmdExecAsync` import changes only command 0x55 / length 3 / power 0 to
power 1. A completion wrapper restores the original payload before Sony's
callback; all callback arguments, return values and unrelated commands pass
through. Only this USB caller is intercepted, not the Syscon provider globally.
Release restores the original import, drains calls and pending completions, then
requests power off. A conflicting hook, undrained callback or failed power write
prevents unsafe unload and retains a retryable lease.

Guards require E1000 GAME context, inactive USB, no foreign accessory function
driver, and the inspected USB module version 1.10 / text size 0x97c8 / resolved
J+NOP import at offset 0x9020. Other layouts fail closed. No flash module is
modified. `esp-adhoc-power.log` records the power API result and boot delay;
`esp-adhoc.log` records acquisition failures. An API success does not measure
rail voltage or prove physical ESP startup.

Host sanitizer checks exercise actual power code with mocked PSP services:
foreign-context and layout refusal, hook/on/delay order, no one-second expiry,
idempotent retry, packet filtering and Sony completion preservation, failed
submission, failed power writes, pending-call retention, hook conflict and exit
cleanup. Hardware validation remains pending.

For the hardware test, keep the existing 3.3 V/GND side-rail wiring, remove the
ESP's external PC power (or isolate its supply), then start Gran Turismo and
enter Ad Hoc. Do not tie the ESP's board 5 V to the PSP. Check that the ESP boots
and stays on beyond one second, repeat the successful car transfer, then leave
the game and verify the rail turns off in XMB. Preserve logs afterward. Test
without manually pre-powering or resetting the ESP so startup ordering is
actually exercised. No ESP firmware update is needed.
