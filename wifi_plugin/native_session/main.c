#include "../../common/esp_diagnostics.h"
/* On-demand user networking session for native netconf; no graphics or LoadExec. */
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <pspnet.h>
#include <pspiofilemgr.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <kubridge.h>
#include <string.h>
#include "../native_control.h"
#include "../../common/link_recovery.h"
#include "../../virtual_net/control.h"
#include "../../virtual_net/app/native_http.h"
#include "../../browser_local/app/adapter.h"
#include "../../browser_local/app/dns.h"
#include "../../browser_local/app/lookup.h"
PSP_MODULE_INFO("ESP Native Net Session",0,0,3);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
static wifi_native_session_control_t *control;
static volatile vnet_start_result_t transport;
static SceUID thread=-1,driver=-1;
static int driver_started,net_owned,inet_owned,resolver_owned,adapter_owned,unsafe,net_live;
static int cancelled(void){return control->cancel;}
static int report(const char *phase,int result)
{(void)phase;control->detail=result;return cancelled()?-1:0;}
static int setup(void)
{
    control->stage=1;
    SceNetMallocStat stats;memset(&stats,0,sizeof(stats));
    if(sceNetGetMallocStat(&stats)<0 || !stats.pool) {
        if(sceNetInit(128*1024,42,4096,42,4096))return -1;
        net_owned=1;
    }
    net_live=1;
    control->stage=2;
    int r=sceNetInetInit();
    if(!r)inet_owned=1;
    else if((unsigned)r!=0x80410201u)return -1; /* owned 6.61 entry2f9f0: already initialized */
    control->stage=3;
    r=sceNetResolverInit();
    if(!r)resolver_owned=1;
    else if((unsigned)r!=0x80000020u)return -1;
    control->stage=4;
    driver=kuKernelLoadModule("ms0:/PSP/GAME/psp-browser/psp_vnet.prx",0,NULL);
    if(driver<0)return -1;
    vnet_start_args_t args={VNET_START_MAGIC,(unsigned)&transport};int status;
    r=sceKernelStartModule(driver,sizeof(args),&args,&status,NULL);
    if(r<0 || status<0)return -1;
    driver_started=1;if(transport.result)return -1;
    control->stage=5;
    unsigned began=sceKernelGetSystemTimeLow();
    unsigned required=VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY;
    while(!cancelled() && (transport.flags&required)!=required && sceKernelGetSystemTimeLow()-began<15000000u)sceKernelDelayThread(10000);
    if(cancelled() || (transport.flags&required)!=required)return -1;
    control->stage=6;
    if(vnet_run_local_http(report) || vnet_run_unbound_local_http(report) || vnet_install_default_route(report))return -1;
    control->stage=7;
    vnet_start_result_t sample;
    if(!vnet_status_snapshot(&sample,&transport) || browser_dns_configure(sample.wifi_info,sample.wifi_info+4))return -1;
    control->stage=8;
    if(browser_lookup_run(report,cancelled))return -1;
    control->stage=9;
    if(adapter_start(&transport))return -1;
    adapter_owned=1;return 0;
}
/* Called only after the virtual driver has stopped USB, or when setup never
 * started it. No file is held across a native cleanup call. */
static int cleanup_logging;
static void cleanup_mark(unsigned step,int result)
{
    if(!ESP1000_DIAGNOSTICS)return;
    if(!cleanup_logging)return;
    char line[80];unsigned used=0;
    const char *names[]={"SESSION_CLEANUP step="," result="};
    unsigned values[]={step,(unsigned)result};
    static const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<2;++i) {
        unsigned n=strlen(names[i]);memcpy(line+used,names[i],n);used+=n;
        for(unsigned j=0;j<8;++j)line[used++]=hex[(values[i]>>(28-4*j))&15];
    }
    line[used++]='\n';
    int fd=sceIoOpen("ms0:/native-wifi.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd>=0){sceIoWrite(fd,line,used);sceIoClose(fd);}
}
static int cleanup(void)
{
    int r;
    if(adapter_owned){if(adapter_pause())return -1;adapter_dispose();adapter_owned=0;}
    if(browser_lookup_cleanup())return -1;
    if(driver_started){int status;r=sceKernelStopModule(driver,0,NULL,&status,NULL);if(r<0 || status<0)return -1;driver_started=0;}
    cleanup_logging=1;cleanup_mark(20,0);
    if(driver>=0){r=sceKernelUnloadModule(driver);cleanup_mark(21,r);if(r<0)return -1;driver=-1;}
    cleanup_mark(22,0);r=browser_dns_restore();cleanup_mark(23,r);if(r)return -1;
    if(resolver_owned){cleanup_mark(24,0);r=sceNetResolverTerm();cleanup_mark(25,r);if(r)return -1;resolver_owned=0;}
    if(inet_owned){cleanup_mark(26,0);r=sceNetInetTerm();cleanup_mark(27,r);if(r)return -1;inet_owned=0;}
    if(net_owned){cleanup_mark(28,0);r=sceNetTerm();cleanup_mark(29,r);if(r)return -1;net_owned=0;net_live=0;}
    cleanup_mark(30,0);return 0;
}
static int worker(SceSize size,void *args)
{
    (void)size;(void)args;control->finished=0;control->result=setup();
    if(!control->result) {
        link_recovery_t recovery={0};
        control->stage=10;control->alive=1;
        control->info_callback=(unsigned)adapter_get_info;
        __sync_synchronize();control->ready=1;
        while(!cancelled()) {
            unsigned required=VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY;
            int ready=(transport.flags&required)==required;
            int action=link_recovery_poll(&recovery,sceKernelGetSystemTimeLow(),ready);
            if(action<0){control->result=-1;break;}
            if(!ready && !control->recovering)++control->recoveries;
            control->stage=ready?10:11;control->detail=ready?0:(int)transport.flags;
            control->recovering=!ready;
            __sync_synchronize();control->ready=ready;
            sceKernelDelayThread(20000);
        }
    }
    control->ready=0;control->alive=0;control->recovering=0;
    __sync_synchronize();
    while(control->info_users)sceKernelDelayThread(1000);
    control->info_callback=0;
    if(cleanup()){unsafe=1;control->result=-2;}
    cleanup_mark(31,net_live);
    if(net_live){int r=sceNetFreeThreadinfo(sceKernelGetThreadId());cleanup_mark(32,r);}
    cleanup_mark(33,control->result);
    __sync_synchronize();control->finished=1;return 0;
}
int module_start(SceSize size,void *args)
{
    if(size!=sizeof(unsigned) || !args)return -1;
    unsigned address;memcpy(&address,args,sizeof(address));SceModule bridge;
    if((address&3) || address<0x08800000u || address>0x0c000000u-sizeof(*control) ||
        kuKernelFindModuleByName("ESP WiFi XMB Bridge",&bridge)<0)return -1;
    int found=0;
    for(unsigned i=0;i<bridge.nsegment && i<4;++i)
        if(sizeof(*control)<=bridge.segmentsize[i] && address>=bridge.segmentaddr[i] && address-bridge.segmentaddr[i]<=bridge.segmentsize[i]-sizeof(*control))found=1;
    if(!found)return -1;
    control=(void *)address;
    if(control->magic!=WIFI_NATIVE_SESSION_MAGIC || control->abi!=WIFI_NATIVE_SESSION_ABI)return -1;
    control->cancel=0;control->ready=0;control->result=-1;control->info_callback=0;
    control->alive=control->recovering=control->finished=0;control->recoveries=control->info_users=0;
    thread=sceKernelCreateThread("ESPNativeNetwork",worker,0x28,0x10000,PSP_THREAD_ATTR_USER,NULL);
    if(thread<0)return -1;
    int r=sceKernelStartThread(thread,0,NULL);
    if(r<0){sceKernelDeleteThread(thread);thread=-1;}return r;
}
int module_stop(SceSize size,void *args)
{
    (void)size;(void)args;control->cancel=1;
    /* ModuleMgr cannot wait for cleanup that itself calls ModuleMgr. The
     * bridge requests cancellation and waits outside StopModule first. */
    if(!control->finished)return -2;
    __sync_synchronize();
    if(thread>=0){unsigned timeout=3000000;if(sceKernelWaitThreadEnd(thread,&timeout)<0)return -2;}
    if(unsafe)return -2;
    if(thread>=0){sceKernelDeleteThread(thread);thread=-1;}return 0;
}
