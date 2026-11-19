/* Inventory only: no instruction patches, flash writes or interface calls.
 * Utility loading is a separate, explicitly selected test: Sony module_start
 * code can still touch hardware, so log+sync before entering it. */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psploadcore.h>
#include <psputility_netmodules.h>
#include <kubridge.h>
#include <systemctrl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "native_check.h"

PSP_MODULE_INFO("PSP WiFi Check", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

static volatile int exiting;
static SceUID log_fd = -1;
static int log_failed;

static int exit_callback(int a, int b, void *arg)
{
    (void)a; (void)b; (void)arg;
    exiting = 1;
    return 0;
}

static void record(const char *format, ...)
{
    char line[384];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (count < 0) return;
    if (count >= (int)sizeof(line)) count = sizeof(line) - 1;
    if (log_fd >= 0 && sceIoWrite(log_fd, line, count) != count)
        log_failed = 1;
}

static void checkpoint(void)
{
    if (sceIoSync("ms0:", 0) < 0) log_failed = 1;
}

static int native_report(const char *step, int result)
{
    record("NATIVE %s result=%08x\n", step, result);
    checkpoint();
    pspDebugScreenPrintf("%s: %08x\n", step, result);
    sceKernelDelayThreadCB(1);
    return log_failed || exiting ? -1 : 0;
}

static void modules(const char *phase)
{
    static const char *names[] = {
        "sceNet_Service", "sceWlan_Driver", "sceNet_Library",
        "sceNetInet_Library", "sceNetApctl_Library", "sceNetResolver_Library",
        "sceNetIfhandle_Library", "sceUtility_Driver", "sceSystemControl",
        "SystemControl", "sceVshBridge_Driver", "vsh_module",
        "htmlviewer_plugin_module", "sceHVUI_Module",
        "sceVshHVUtility_Module", "sceVshNetconf_Module"
    };
    record("SNAPSHOT %s\n", phase);
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        SceModule mod;
        memset(&mod, 0, sizeof(mod));
        int r = kuKernelFindModuleByName((char *)names[i], &mod);
        record("MODULE %s result=%08x", names[i], r);
        if (r >= 0) {
            record(" uid=%08x attr=%04x version=%u.%u text=%08x+%x"
                   " exports=%08x+%x imports=%08x+%x segments=%u\n",
                   mod.modid, mod.attribute, mod.version[1], mod.version[0],
                   mod.text_addr, mod.text_size,
                   (unsigned)mod.ent_top, (unsigned)mod.ent_size,
                   (unsigned)mod.stub_top, (unsigned)mod.stub_size, mod.nsegment);
            /* Addresses are diagnostics only. Never dereference kernel pointers
             * copied through kubridge from this user-mode program. */
        } else record("\n");
    }
    checkpoint();
}

static void files(const char *directory)
{
    SceUID fd = sceIoDopen(directory);
    record("DIRECTORY %s result=%08x\n", directory, fd);
    if (fd < 0) return;
    SceIoDirent entry;
    int r;
    unsigned count = 0;
    do {
        memset(&entry, 0, sizeof(entry));
        r = sceIoDread(fd, &entry);
        if (r > 0 && strstr(entry.d_name, ".prx"))
            record("FILE %s/%s size=%llu\n", directory, entry.d_name,
                   (unsigned long long)entry.d_stat.st_size);
        /* Bound even an unexpected filesystem directory. */
    } while (r > 0 && ++count < 2048 && !exiting);
    record("DIRECTORY_END result=%08x entries=%u\n", r, count);
    sceIoDclose(fd);
}

int main(void)
{
    int owned_common = 0, owned_inet = 0, tested = 0, native_tested = 0;
    unsigned previous = 0;
    pspDebugScreenInit();
    pspDebugScreenPrintf("PSP Wi-Fi Compatibility Check v2\n\n");
    log_fd = sceIoOpen("ms0:/net-probe.log",
                       PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0600);
    if (log_fd < 0) {
        pspDebugScreenPrintf("Cannot write ms0:/net-probe.log (%08x).\n", log_fd);
        log_failed = 1;
    }
    SceUID cb = sceKernelCreateCallback("NetProbeExit", exit_callback, NULL);
    int registered = cb < 0 ? cb : sceKernelRegisterExitCallback(cb);
    record("\nBEGIN net-probe-v2 uptime_us=%llu devkit=%08x model=%d"
           " cfw=%08x minor=%08x callback=%08x\n",
           (unsigned long long)sceKernelGetSystemTimeWide(), sceKernelDevkitVersion(),
           kuKernelGetModel(), sctrlHENGetVersion(), sctrlHENGetMinorVersion(), registered);
    if (registered < 0)
        pspDebugScreenPrintf("HOME callback unavailable; use CIRCLE.\n");
    files("flash0:/kd");
    files("flash0:/vsh/module");
    modules("initial");
    pspDebugScreenPrintf("Inventory saved to ms0:/net-probe.log\n"
        "No flash files or profiles are changed.\n\n"
        "CIRCLE / HOME: exit after inventory only\n"
        "CROSS: test stock COMMON / INET loading\n"
        "SQUARE: then test native UDP loopback\n\n"
        "The load test runs Sony startup code. If it\n"
        "hangs, restart and retain the log.\n");
    while (!exiting) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) break;
        if ((pressed & PSP_CTRL_CROSS) && !tested && !log_failed) {
            tested = 1;
            record("LOAD_BEGIN COMMON\n");
            checkpoint();
            if (log_failed) break;
            pspDebugScreenPrintf("Loading COMMON...\n");
            int r = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
            record("LOAD_END COMMON result=%08x\n", r);
            pspDebugScreenPrintf("COMMON: %08x\n", r);
            owned_common = r == 0;
            modules("after-common");
            /* Do not continue after failed prerequisites, including an already
             * loaded result. Do not claim ownership of someone else's modules. */
            sceKernelDelayThreadCB(1);
            if (owned_common && !exiting && !log_failed) {
                record("LOAD_BEGIN INET\n");
                checkpoint();
                if (log_failed) break;
                pspDebugScreenPrintf("Loading INET...\n");
                r = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
                record("LOAD_END INET result=%08x\n", r);
                pspDebugScreenPrintf("INET: %08x\n", r);
                owned_inet = r == 0;
                modules("after-inet");
            }
            pspDebugScreenPrintf("Module test recorded. SQUARE: native UDP test.\nCIRCLE / HOME exits.\n");
        }
        if ((pressed & PSP_CTRL_SQUARE) && owned_common && owned_inet
            && !native_tested && !log_failed && !exiting) {
            native_tested = 1;
            int r = native_socket_check(native_report);
            record("NATIVE_CHECK result=%08x\n", r);
            if (r == -2) {
                owned_inet = owned_common = 0;
                pspDebugScreenPrintf("Native cleanup failed; modules retained until exit.\n");
            }
            pspDebugScreenPrintf("Loopback %s. CIRCLE / HOME exits.\n",
                                 r == 0 ? "passed" : "failed (see log)");
        }
        if (log_failed) {
            pspDebugScreenPrintf("Log write/sync failed. Exit and check memory stick.\n");
            break;
        }
        sceKernelDelayThreadCB(20000);
    }
    if (owned_inet) {
        record("UNLOAD_BEGIN INET\n");
        checkpoint();
        int r = sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        record("UNLOAD_END INET result=%08x\n", r);
        /* Keep COMMON if its dependent module could not be removed. */
        if (r < 0) owned_common = 0;
    }
    if (owned_common) {
        record("UNLOAD_BEGIN COMMON\n");
        checkpoint();
        record("UNLOAD_END COMMON result=%08x\n",
               sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON));
    }
    record("END net-probe-v2\n");
    if (log_fd >= 0) {
        sceIoClose(log_fd);
        checkpoint();
    }
    sceKernelExitGame();
    return 0;
}
