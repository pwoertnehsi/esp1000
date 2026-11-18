/*
 * accpower - bring up the PSP's USB accessory mode the way the camera app does,
 * then simply STAY there, and log every state change.
 *
 * Purpose: find out whether the accessory power rail (the two pogo pads) and the
 * "activated, waiting for VBUS" state last longer than the ~1 s the camera app
 * allows. Measure the pad with a multimeter while this runs.
 *
 * What it does (mirrors what usblog saw the camera app do):
 *   load usbacc + usbcam modules -> start USBBusDriver, USBAccBaseDriver,
 *   USBCamDriver, USBCamMicDriver -> sceUsbActivate(0x282) -> poll and log.
 * CIRCLE or HOME: stop (deactivates and unloads cleanly).
 * SQUARE: re-arm (deactivate, wait, activate again) without restarting the app,
 *         handy after an ESP test run dropped the connection.
 *
 * Disable RemoteJoyLite first (it clashed with the camera app before).
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <pspusb.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

PSP_MODULE_INFO("accpower", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

/* Same prototypes as psputility_usbmodules.h, declared here in case that header
 * is not pulled in by psputility.h on your SDK. */
extern int sceUtilityLoadUsbModule(int module);
extern int sceUtilityUnloadUsbModule(int module);

#define USB_MODULE_ACC 2
#define USB_MODULE_CAM 4
#define ACC_PID        0x282
#define LOG_PATH       "ms0:/accpower.txt"

static const char *drv_names[] = {
    "USBBusDriver", "USBAccBaseDriver", "USBCamDriver", "USBCamMicDriver"
};
#define NUM_DRV 4

static volatile int running = 1;
static u32 t0;

static int exit_cb(int a, int b, void *c) { (void)a; (void)b; (void)c; running = 0; return 0; }
static int cb_thread(SceSize args, void *argp)
{
    int cb;
    (void)args; (void)argp;
    cb = sceKernelCreateCallback("exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

static void say(const char *fmt, ...)
{
    char line[160], out[200];
    va_list ap;
    SceUID fd;
    u32 t = sceKernelGetSystemTimeLow() - t0;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    snprintf(out, sizeof(out), "%4u.%03u %s\n", (unsigned)(t / 1000000), (unsigned)((t / 1000) % 1000), line);
    pspDebugScreenPrintf("%s", out);
    fd = sceIoOpen(LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) { sceIoWrite(fd, out, strlen(out)); sceIoClose(fd); }
}

static void flags(int s, char *out)
{
    if (s < 0) { strcpy(out, "ERR"); return; }
    out[0] = (s & 0x200) ? 'A' : '-';   /* activated */
    out[1] = (s & 0x020) ? 'C' : '-';   /* cable / VBUS seen */
    out[2] = (s & 0x002) ? 'E' : '-';   /* established (enumerated) */
    out[3] = 0;
}

int main(int argc, char **argv)
{
    int prev_state = 0x7fffffff, prev_drv[NUM_DRV], i, r;
    SceCtrlData pad;
    SceUID th;
    int sq_prev = 0;
    (void)argc; (void)argv;

    pspDebugScreenInit();
    th = sceKernelCreateThread("exit_cb", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
    t0 = sceKernelGetSystemTimeLow();
    for (i = 0; i < NUM_DRV; i++) prev_drv[i] = 0x7fffffff;

    say("accpower: CIRCLE or HOME to stop");
    r = sceUtilityLoadUsbModule(USB_MODULE_ACC); say("load ACC module: 0x%08x", r);
    r = sceUtilityLoadUsbModule(USB_MODULE_CAM); say("load CAM module: 0x%08x", r);
    for (i = 0; i < NUM_DRV; i++) {
        r = sceUsbStart(drv_names[i], 0, 0);
        say("start %-18s: 0x%08x", drv_names[i], r);
    }
    r = sceUsbActivate(ACC_PID);
    say("activate 0x%03x: 0x%08x  -> measure the pad now", ACC_PID, r);

    while (running) {
        int s = sceUsbGetState();
        char f[4];
        if (s != prev_state) { flags(s, f); say("USB state 0x%08x [%s]", s, f); prev_state = s; }
        for (i = 0; i < NUM_DRV; i++) {
            int d = sceUsbGetDrvState(drv_names[i]);
            if (d != prev_drv[i]) { say("drv %-18s 0x%08x", drv_names[i], d); prev_drv[i] = d; }
        }
        sceCtrlPeekBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_CIRCLE) break;
        {
            int sq = (pad.Buttons & PSP_CTRL_SQUARE) != 0;
            if (sq && !sq_prev) {
                say("re-arm: deactivate");
                sceUsbDeactivate(ACC_PID);
                sceKernelDelayThread(300000);
                r = sceUsbActivate(ACC_PID);
                say("re-arm: activate 0x%08x", r);
            }
            sq_prev = sq;
        }
        sceKernelDelayThread(20000);
    }

    say("stopping");
    sceUsbDeactivate(ACC_PID);
    for (i = NUM_DRV - 1; i >= 0; i--) sceUsbStop(drv_names[i], 0, 0);
    sceUtilityUnloadUsbModule(USB_MODULE_CAM);
    sceUtilityUnloadUsbModule(USB_MODULE_ACC);
    say("done");
    sceKernelDelayThread(500000);
    sceKernelExitGame();
    return 0;
}
