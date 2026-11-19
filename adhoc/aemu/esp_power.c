#include "../../common/esp_diagnostics.h"
/* SPDX-License-Identifier: GPL-3.0-or-later
 * E1000 accessory power lease (GAME by default, VSH opt-in at build time). No camera function driver is started.
 * The guarded USB-local import hook retains power across USB deactivation;
 * other Syscon clients, commands and completion callbacks are untouched. */
#include <pspkernel.h>
#include <pspsdk.h>
#include <pspinit.h>
#include <psploadcore.h>
#include <pspusb.h>
#include <psputilsforkernel.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <pspsysmem_kernel.h>
#include <systemctrl.h>
#include "esp_power_policy.h"
#ifndef ESP_POWER_CONTEXT
#define ESP_POWER_CONTEXT PSP_INIT_KEYCONFIG_GAME
#endif
#ifndef ESP_POWER_MODULE
#define ESP_POWER_MODULE SceModule2
#endif
#ifndef ESP_POWER_TAG
#define ESP_POWER_TAG "ADHOC_POWER"
#endif
#ifndef ESP_POWER_LOG
#define ESP_POWER_LOG "ms0:/esp-adhoc-power.log"
#endif
extern int sceSysconCtrlUsbPower(signed char power);
static volatile int owned, changing, calls;
static unsigned *stub, saved[2], patched[2];
static int (*original_async)(void *, int, void *, void *);
static volatile int pending, callback_slot;
static int (*sony_callback)(void *, void *);
static void *sony_argument;
static int held_complete(void *packet, void *arg)
{
    (void)arg;
    int (*callback)(void *, void *) = sony_callback;
    void *argument = sony_argument;
    /* Sony must observe the off request it submitted, not our rail override.
     * Its completion state machine otherwise treats this as a power-on event. */
    ((unsigned char *)packet)[14] = 0;
    __sync_lock_release(&callback_slot);
    int result = callback ? callback(packet, argument) : 0;
    __sync_sub_and_fetch(&pending, 1);
    return result;
}
static int hold_async(void *packet, int flags, void *callback, void *arg)
{
    __sync_add_and_fetch(&calls, 1);
    int held = esp_power_hold_packet(packet, owned);
    if (held) {
        if (__sync_lock_test_and_set(&callback_slot, 1)) {
            ((unsigned char *)packet)[14] = 0;
            __sync_sub_and_fetch(&calls, 1);
            return -1;
        }
        sony_callback = callback;
        sony_argument = arg;
        __sync_add_and_fetch(&pending, 1);
    }
    int result = original_async(packet, flags, held ? (void *)held_complete : callback, held ? NULL : arg);
    if (held && result < 0) {
        ((unsigned char *)packet)[14] = 0;
        __sync_sub_and_fetch(&pending, 1);
        __sync_lock_release(&callback_slot);
    }
    __sync_sub_and_fetch(&calls, 1);
    return result;
}
static void flush(void)
{
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}
static void log_power(const char *event, int result)
{
    if(!ESP1000_DIAGNOSTICS)return;
    char line[80];
    int n = snprintf(line, sizeof(line), "%s %s=%08x\n", ESP_POWER_TAG, event, (unsigned)result);
    int fd = sceIoOpen(ESP_POWER_LOG, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0600);
    if (fd >= 0) { sceIoWrite(fd, line, n); sceIoClose(fd); }
}
static int acquire(void)
{
    if (owned) return 0;
    if (sceKernelGetModel() != 10 || sceKernelInitKeyConfig() != ESP_POWER_CONTEXT)
        return -1;
    const char *foreign[] = {"USBCamDriver", "USBCamMicDriver", "USBStor_Driver", "USBGpsDriver", "USBHostFSDriver"};
    if (sceUsbGetState() & PSP_USB_ACTIVATED) return -1;
    for (unsigned i = 0; i < sizeof(foreign)/sizeof(foreign[0]); ++i)
        if (sceUsbGetDrvState(foreign[i]) == 1) return -1;
    ESP_POWER_MODULE *module = (ESP_POWER_MODULE *)sceKernelFindModuleByName("sceUSB_Driver");
    /* Owned 6.61 USB module: version 1.10, text 0x97c8. Fail closed on a
     * different layout. Offset 0x9020 is its sceSysconCmdExecAsync import. */
    if (!module || module->text_size != 0x97c8 || module->version[0] != 10 || module->version[1] != 1)
        return -1;
    stub = (unsigned *)(module->text_addr + 0x9020);
    if ((stub[0] & 0xfc000000) != 0x08000000 || stub[1] != 0) return -1;
    saved[0] = stub[0]; saved[1] = stub[1];
    original_async = (void *)(((uintptr_t)stub & 0xf0000000) | ((saved[0] & 0x03ffffff) << 2));
    unsigned syscon_async = sctrlHENFindFunction("sceSYSCON_Driver", "sceSyscon_driver", 0x3ac3d2a4);
    if (!syscon_async || (uintptr_t)original_async != syscon_async) return -1;
    patched[0] = 0x08000000 | (((uintptr_t)hold_async >> 2) & 0x03ffffff);
    patched[1] = 0;
    int intr = sceKernelCpuSuspendIntr();
    owned = 1;
    stub[0] = patched[0]; stub[1] = patched[1]; flush();
    sceKernelCpuResumeIntr(intr);
    int result = sceSysconCtrlUsbPower(1);
    log_power("ON", result);
    if (result < 0) {
        intr = sceKernelCpuSuspendIntr();
        stub[0] = saved[0]; stub[1] = saved[1]; owned = 0; flush();
        sceKernelCpuResumeIntr(intr);
        return result;
    }
    /* Let the rail-powered ESP boot before asserting the USB accessory link.
     * This delay occurs once per lease, never on USB handoff/reconnect. */
    sceKernelDelayThread(2000000);
    log_power("BOOT_WAIT_DONE", 0);
    return 0;
}
int espAdhocPowerAcquire(void)
{
    int k1 = pspSdkSetK1(0);
    if (__sync_lock_test_and_set(&changing, 1)) { pspSdkSetK1(k1); return -1; }
    int result = acquire();
    __sync_lock_release(&changing);
    pspSdkSetK1(k1);
    return result;
}
int espAdhocPowerRelease(void)
{
    int k1 = pspSdkSetK1(0);
    if (__sync_lock_test_and_set(&changing, 1)) { pspSdkSetK1(k1); return -1; }
    int result = 0;
    if (owned) {
        int intr = sceKernelCpuSuspendIntr();
        if (stub[0] != patched[0] || stub[1] != patched[1]) {
            sceKernelCpuResumeIntr(intr);
            result = -1; /* Another hook owns our trampoline: refuse unload. */
        } else {
            stub[0] = saved[0]; stub[1] = saved[1]; flush();
            sceKernelCpuResumeIntr(intr);
            unsigned began = sceKernelGetSystemTimeLow();
            while ((calls || pending) && sceKernelGetSystemTimeLow() - began < 1000000u)
                sceKernelDelayThread(1000);
            if (calls || pending) result = -1;
            else {
                result = sceSysconCtrlUsbPower(0);
                if (result >= 0) owned = 0;
                else { /* Keep a valid retryable lease on a failed write. */
                    intr = sceKernelCpuSuspendIntr();
                    stub[0] = patched[0]; stub[1] = patched[1]; flush();
                    sceKernelCpuResumeIntr(intr);
                }
            }
            if (calls || pending) {
                intr = sceKernelCpuSuspendIntr();
                stub[0] = patched[0]; stub[1] = patched[1]; flush();
                sceKernelCpuResumeIntr(intr);
            }
        }
        log_power("OFF", result);
    }
    __sync_lock_release(&changing);
    pspSdkSetK1(k1);
    return result;
}

int module_reboot_before(SceSize args, void *argp)
{
    (void)args; (void)argp;
    return espAdhocPowerRelease();
}
