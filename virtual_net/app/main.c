#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility_netmodules.h>
#ifdef VNET_USB_LAB
#include <psputility_usbmodules.h>
static int usb_module_owned;
#endif
#include <pspnet.h>
#include <pspnet_inet.h>
#include <kubridge.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../control.h"
#include "lab_client.h"
#ifdef VNET_USB_LAB
#include "native_http.h"
#endif

PSP_MODULE_INFO("PSP Virtual Interface Test", 0, 0, 8);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

static volatile int exiting;
enum {JOB_NONE, JOB_REGISTER, JOB_LAB, JOB_SHUTDOWN, JOB_INTERNET, JOB_LOCAL_TCP};
static SceUID job_thread = -1, abort_thread = -1;
static volatile int job_done, job_result, job_cancel, abort_done;
static int job_kind, abort_requested, shutdown_failed, stall_reported;
static const char * volatile job_phase = "idle";
static volatile unsigned phase_time;
static char driver_path[512];
static int common_owned, inet_module_owned, net_owned, inet_owned, driver_started;
static SceUID driver = -1;
static volatile vnet_start_result_t driver_result;

static int exit_callback(int a, int b, void *arg)
{ (void)a; (void)b; (void)arg; exiting = 1; return 0; }

static void say(const char *format, ...)
{
    char text[256];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    pspDebugScreenPrintf("%s", text);
    SceUID fd = sceIoOpen("ms0:/psp-vnet.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0600);
    if (fd >= 0) { sceIoWrite(fd, text, strlen(text)); sceIoClose(fd); sceIoSync("ms0:", 0); }
}

static int stop_driver(int pool_gone)
{
    int status = 0;
    unsigned magic = VNET_NETTERM_MAGIC;
    int r = sceKernelStopModule(driver, pool_gone ? sizeof(magic) : 0,
                                pool_gone ? &magic : NULL, &status, NULL);
    say("Stop virtual driver%s: %08x (status %08x)\n",
        pool_gone ? " after NET term" : "", r, status);
    if (r < 0 || status == VNET_STOP_UNSAFE) return VNET_STOP_UNSAFE;
    if (status != 0) return -1;
    driver_started = 0;
    return 0;
}

static int unload_driver(void)
{
    if (driver < 0) return 0;
    int r = sceKernelUnloadModule(driver);
    say("Unload virtual driver: %08x\n", r);
    if (r < 0) return -1;
    driver = -1;
    return 0;
}

static int shutdown_native(void)
{
    int deferred = 0, r;
    /* Keep NET's thread and pool alive until the synchronous detach finishes. */
    if (driver_started) {
        int stopped = stop_driver(0);
        if (stopped == VNET_STOP_UNSAFE) return -1;
        if (stopped < 0) deferred = 1;
        else if (unload_driver() < 0) return -1;
    } else if (unload_driver() < 0) return -1;
    if (inet_owned) {
        say("INET_TERM_BEGIN\n");
        r = sceNetInetTerm(); say("INET_TERM_END %08x\n", r);
        if (r != 0) return -1;
        inet_owned = 0;
    }
    if (net_owned) {
        say("NET_TERM_BEGIN\n");
        r = sceNetTerm(); say("NET_TERM_END %08x\n", r);
        if (r != 0) return -1;
        net_owned = 0;
    }
    /* Failed native detach/destroy must never free an allocation after NET's
     * allocator has disappeared. Tell our driver to forget, not free, only
     * after successful sceNetTerm has cleared the native thread/registry/pool. */
    if (deferred && (stop_driver(1) < 0 || unload_driver() < 0)) return -1;
    if (inet_module_owned) {
        r = sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        say("Unload INET modules: %08x\n", r);
        if (r != 0) return -1;
        inet_module_owned = 0;
    }
#ifdef VNET_USB_LAB
    if (usb_module_owned) {
        r = sceUtilityUnloadUsbModule(PSP_USB_MODULE_ACC);
        say("Unload accessory module: %08x\n", r);
        if (r != 0) return -1;
        usb_module_owned = 0;
    }
#endif
    if (common_owned) {
        r = sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        say("Unload COMMON modules: %08x\n", r);
        if (r != 0) return -1;
        common_owned = 0;
    }
    return 0;
}

static void start_native(const char *path)
{
    int r, status = 0;
    say("Load COMMON...\n");
    r = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    say("COMMON: %08x\n", r);
    if (r != 0) return;
    common_owned = 1;
    say("Load INET...\n");
    r = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    say("INET: %08x\n", r);
    if (r != 0) return;
    inet_module_owned = 1;
    say("NET_INIT_BEGIN\n");
    r = sceNetInit(128 * 1024, 42, 4096, 42, 4096);
    say("NET_INIT_END %08x\n", r);
    if (r != 0) return;
    net_owned = 1;
    say("INET_INIT_BEGIN\n");
    r = sceNetInetInit(); say("INET_INIT_END %08x\n", r);
    if (r != 0) return;
    inet_owned = 1;
#ifdef VNET_USB_LAB
    r = sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC);
    say("Load accessory module: %08x\n", r);
    if (r != 0) return;
    usb_module_owned = 1;
#endif
    driver = kuKernelLoadModule(path, 0, NULL);
    say("Load virtual driver: %08x\n", driver);
    if (driver < 0) return;
    driver_result.result = -1;
    driver_result.flags = 0;
    vnet_start_args_t args = {VNET_START_MAGIC, (unsigned)&driver_result};
    r = sceKernelStartModule(driver, sizeof(args), &args, &status, NULL);
    say("Start virtual driver: %08x (status %08x)\n", r, status);
    if (r < 0 || status != 0) return;
    driver_started = 1;
    say("Driver result: %08x flags=%08x\n", driver_result.result, driver_result.flags);
    if (driver_result.result == 0 && (driver_result.flags & (VNET_REGISTERED | VNET_CALLBACK_OK)) == (VNET_REGISTERED | VNET_CALLBACK_OK))
        say("Virtual interface registered; callback verified.\n");
    else say("Registration failed or refused; retain the log.\n");
    say("SQUARE: native ARP/UDP to the local lab gateway.\n"
#ifdef VNET_USB_LAB
        "USB: SQUARE=UDP, TRIANGLE=local TCP check, START=internet after TCP passes.\n");
#else
        "This fixture does not use USB or ESP Wi-Fi.\n");
#endif
}

static int lab_report(const char *step, int result)
{
    job_phase = step;
    phase_time = sceKernelGetSystemTimeLow();
    say("%s: %08x\n", step, result);
    return exiting || job_cancel ? -1 : 0;
}

static int run_job(SceSize size, void *args)
{
    (void)size; (void)args;
    int result = 0;
    if (job_kind == JOB_REGISTER) start_native(driver_path);
    else if (job_kind == JOB_LAB) result = vnet_run_lab_client(lab_report);
#ifdef VNET_USB_LAB
    else if (job_kind == JOB_LOCAL_TCP) result=vnet_run_local_http(lab_report);
    else if (job_kind == JOB_INTERNET) {
        unsigned char ip[4];memcpy(ip,(const void *)driver_result.remote_ip,4);
        result=vnet_run_native_http(lab_report,ip);
    }
#endif
    else if (job_kind == JOB_SHUTDOWN) result = shutdown_native();
    if (net_owned) {
        int r = sceNetFreeThreadinfo(sceKernelGetThreadId());
        say("Release job network context: %08x\n", r);
    }
    job_result = result;
    __sync_synchronize();
    job_done = 1; /* No native calls after publishing completion. */
    return 0;
}

static int request_abort(SceSize size, void *args)
{
    (void)size; (void)args;
    /* This call may itself need native locks; keep it off the UI thread too. */
    int r = sceNetThreadAbort(job_thread);
    say("Native thread abort result: %08x\n", r);
    r = sceNetFreeThreadinfo(sceKernelGetThreadId());
    say("Release abort network context: %08x\n", r);
    __sync_synchronize();
    abort_done = 1;
    return 0;
}

static int begin_job(int kind)
{
    job_kind = kind;
    job_done = 0; job_result = -1; job_cancel = 0;
    abort_requested = stall_reported = 0;
    job_phase = kind == JOB_LOCAL_TCP ? "LOCAL_TCP" : kind == JOB_INTERNET ? "INTERNET" : kind == JOB_LAB ? "LAB_START" : kind == JOB_REGISTER ? "REGISTER" : "SHUTDOWN";
    phase_time = sceKernelGetSystemTimeLow();
    job_thread = sceKernelCreateThread("VirtualNetJob", run_job, 0x28, 0x10000,
                                       PSP_THREAD_ATTR_USER, NULL);
    if (job_thread < 0) { say("Create job failed: %08x\n", job_thread); return -1; }
    int r = sceKernelStartThread(job_thread, 0, NULL);
    if (r < 0) {
        say("Start job failed: %08x\n", r);
        sceKernelDeleteThread(job_thread); job_thread = -1;
        return -1;
    }
    return 0;
}

static void show_wait_state(void)
{
    SceKernelThreadInfo info;
    memset(&info, 0, sizeof(info)); info.size = sizeof(info);
    int r = sceKernelReferThreadStatus(job_thread, &info);
    say("Job stalled at %s: query=%08x thread=%08x status=%08x wait_type=%u wait_id=%08x\n",
        job_phase, r, job_thread, r == 0 ? info.status : 0,
        r == 0 ? info.waitType : 0, r == 0 ? info.waitId : 0);
}

static void cancel_lab(void)
{
    if (abort_requested || (job_kind != JOB_LAB && job_kind != JOB_INTERNET && job_kind != JOB_LOCAL_TCP) || !net_owned || job_done) return;
    abort_requested = 1; job_cancel = 1;
    say("Cancelling native packet test at %s...\n", job_phase);
    show_wait_state();
    abort_done = 0;
    abort_thread = sceKernelCreateThread("VirtualNetAbort", request_abort, 0x20, 0x4000,
                                         PSP_THREAD_ATTR_USER, NULL);
    if (abort_thread < 0) { say("Create abort thread failed: %08x\n", abort_thread); return; }
    int r = sceKernelStartThread(abort_thread, 0, NULL);
    if (r < 0) {
        say("Start abort thread failed: %08x\n", r);
        sceKernelDeleteThread(abort_thread); abort_thread = -1;
    }
}

int main(int argc, char **argv)
{
    int tested = 0, lab_tested = 0;
#ifdef VNET_USB_LAB
    int internet_tested=0, local_tested=0, local_passed=0;unsigned previous_uplink=~0u;
#endif
    unsigned previous = 0;
    pspDebugScreenInit();
    say("\nPSP Virtual Interface Test v0.8\n"
        "6.61 ARK-5 / PSP-E1000\n"
        "CROSS: register; SQUARE: packet lab\n"
        "CIRCLE / HOME: cancel and exit\n"
        "Log: ms0:/psp-vnet.log\n\n");
    SceUID cb = sceKernelCreateCallback("VirtualNetExit", exit_callback, NULL);
    int r = cb < 0 ? cb : sceKernelRegisterExitCallback(cb);
    if (r < 0) say("HOME callback failed (%08x); use CIRCLE.\n", r);
    if (argc < 1 || !argv[0] || strlen(argv[0]) >= sizeof(driver_path)) goto bad_path;
    strcpy(driver_path, argv[0]);
    char *slash = strrchr(driver_path, '/');
    if (!slash || (size_t)(slash - driver_path) + sizeof("/psp_vnet.prx") > sizeof(driver_path)) goto bad_path;
    strcpy(slash + 1, "psp_vnet.prx");
    for (;;) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) exiting = 1;
#ifdef VNET_USB_LAB
        unsigned uplink=driver_result.flags&(VNET_USB_READY|VNET_UPLINK_READY|VNET_REMOTE_READY);
        if(driver_started && uplink!=previous_uplink) {
            previous_uplink=uplink;
            say("Adapter: USB=%d Wi-Fi=%d target=%d\n",!!(uplink&VNET_USB_READY),!!(uplink&VNET_UPLINK_READY),!!(uplink&VNET_REMOTE_READY));
        }
#endif
        /* Retire the abort helper first; it can still reference the job UID or
         * native pool even when the target network operation has returned. */
        if (abort_thread >= 0 && abort_done) {
            sceKernelWaitThreadEnd(abort_thread, NULL);
            sceKernelDeleteThread(abort_thread); abort_thread = -1;
        }
        if (job_thread >= 0 && job_done && abort_thread < 0) {
            sceKernelWaitThreadEnd(job_thread, NULL);
            sceKernelDeleteThread(job_thread); job_thread = -1;
            if (job_kind == JOB_LAB)
#ifdef VNET_USB_LAB
                say("USB packet lab %s. CIRCLE / HOME exits.\n",
                    job_result == 0 ? "passed" : "failed");
#else
                say("Local packet lab %s. CIRCLE / HOME exits.\n",
                    job_result == 0 ? "passed" : "failed or cancelled; retain the log");
#endif
#ifdef VNET_USB_LAB
            if(job_kind==JOB_LOCAL_TCP) {
                local_passed=job_result==0;
                say("Local TCP %s. %s\n",local_passed?"passed":"failed",local_passed?"START tests internet download.":"Stop here and retain the log.");
            }
            if(job_kind==JOB_INTERNET) say("Native HTTP %s. CIRCLE / HOME exits.\n",job_result==0?"saved to ms0:/native-http.html":"failed; retain log");
#endif
            if (job_kind == JOB_SHUTDOWN) {
                if (job_result == 0) {
                    say("END: returning to XMB\n");
                    sceKernelExitGame();
                    return 0;
                }
                shutdown_failed = 1;
                say("Native shutdown failed. Restart the console; resources retained.\n");
            }
        }
        if (job_thread >= 0 && !job_done) {
            unsigned elapsed = sceKernelGetSystemTimeLow() - phase_time;
            if (exiting) cancel_lab();
            if (elapsed >= 5000000u && !stall_reported) {
                stall_reported = 1;
                show_wait_state();
                cancel_lab();
                say("Native call is stalled. Controls remain active.\n"
                    "If cancellation cannot return, restart the console.\n");
            }
        }
        if (job_thread < 0 && abort_thread < 0 && !shutdown_failed) {
            if (exiting) {
                if (begin_job(JOB_SHUTDOWN) < 0) {
                    shutdown_failed = 1;
                    say("Could not start cleanup. Restart the console.\n");
                }
            } else if ((pressed & PSP_CTRL_CROSS) && !tested) {
                tested = 1;
                begin_job(JOB_REGISTER);
#ifdef VNET_USB_LAB
            } else if ((pressed & PSP_CTRL_TRIANGLE) && !local_tested && driver_started) {
                if(!(driver_result.flags&VNET_USB_READY)) say("USB not ready; TRIANGLE retries.\n");
                else {local_tested=1;begin_job(JOB_LOCAL_TCP);}
            } else if ((pressed & PSP_CTRL_START) && !internet_tested && driver_started) {
                if(!local_passed) {say("Run TRIANGLE local TCP check first.\n");sceKernelDelayThreadCB(20000);continue;}
                if((driver_result.flags&(VNET_USB_READY|VNET_UPLINK_READY|VNET_REMOTE_READY)) !=
                   (VNET_USB_READY|VNET_UPLINK_READY|VNET_REMOTE_READY))
                    say("Waiting for USB, Wi-Fi and target IP. Enable hotspot; TRIANGLE retries.\n");
                else { internet_tested=1;begin_job(JOB_INTERNET); }
#endif
            } else if ((pressed & PSP_CTRL_SQUARE) && !lab_tested && driver_started
                       && driver_result.result == 0 && (driver_result.flags & VNET_LAB_BACKEND)) {
#ifdef VNET_USB_LAB
                if (!(driver_result.flags & VNET_USB_READY)) {
                    say("USB adapter is not ready yet. Wait for USB_PACKET_READY=1, then press SQUARE.\n");
                    sceKernelDelayThreadCB(20000);
                    continue;
                }
#endif
                lab_tested = 1;
                begin_job(JOB_LAB);
            }
        }
        sceKernelDelayThreadCB(20000);
    }
bad_path:
    say("Invalid launch path; returning to XMB.\n");
    sceKernelExitGame();
    return 0;
}
