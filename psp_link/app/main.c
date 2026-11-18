#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility_usbmodules.h>
#include <kubridge.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

PSP_MODULE_INFO("PSP ESP Link Loader", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

static volatile int exit_requested;

static int exit_callback(int a, int b, void *arg)
{
    (void)a; (void)b; (void)arg;
    exit_requested = 1;
    return 0;
}

static void say(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    pspDebugScreenPrintf("%s", line);
    SceUID fd = sceIoOpen("ms0:/psp-link.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, line, strlen(line));
        sceIoClose(fd);
    }
}

int main(int argc, char **argv)
{
    char path[512];
    int status = 0, r;
    SceUID module = -1;
    int started = 0;
    pspDebugScreenInit();
    say("PSP ESP Link loader v5\n");
    SceUID cb = sceKernelCreateCallback("LinkUserExit", exit_callback, NULL);
    if (cb < 0) {
        say("Create exit callback failed: %08x\n", cb);
        goto fail;
    }
    r = sceKernelRegisterExitCallback(cb);
    say("Register user exit callback: %08x\n", r);
    if (r < 0) goto fail;
    r = sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC);
    say("Load accessory module: %08x\n", r);
    if (r < 0) goto fail;
    if (argc < 1 || !argv[0] || strlen(argv[0]) >= sizeof(path)) {
        say("Missing or oversized EBOOT launch path\n");
        goto fail;
    }
    strcpy(path, argv[0]);
    char *slash = strrchr(path, '/');
    if (!slash || (size_t)(slash - path) + sizeof("/psp_link.prx") > sizeof(path)) {
        say("Cannot construct link driver path\n");
        goto fail;
    }
    strcpy(slash + 1, "psp_link.prx");
    say("Driver path: %s\n", path);
    module = kuKernelLoadModule(path, 0, NULL);
    say("Load link driver: %08x\n", module);
    if (module < 0) goto fail;
    r = sceKernelStartModule(module, 0, NULL, &status, NULL);
    say("Start link driver: %08x (status %08x)\n", r, status);
    if (r < 0 || status < 0) goto fail;
    started = 1;
    goto wait_exit;
fail:
    say("Startup failed. Codes saved to ms0:/psp-link.log. CIRCLE exits.\n");
wait_exit:
    /* Keep a user thread alive to own HOME/CIRCLE and ExitGame. The kernel
     * worker must finish before ExitGame invokes module shutdown callbacks. */
    while (!exit_requested) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_CIRCLE) break;
        sceKernelDelayThreadCB(20000);
    }
    if (started) {
        r = sceKernelStopModule(module, 0, NULL, &status, NULL);
        say("Stop link module: %08x (status %08x)\n", r, status);
        if (r >= 0 && status >= 0) {
            r = sceKernelUnloadModule(module);
            say("Unload link module: %08x\n", r);
        }
    } else if (module >= 0) {
        r = sceKernelUnloadModule(module);
        say("Unload unstarted link module: %08x\n", r);
    }
    say("User launcher returning to XMB\n");
    sceKernelExitGame();
    return 0;
}
