/* USB-only native settings service. It does not initialize or replace Sony NET.
 * Runs only for an explicit scan/profile request; releases USB on completion. */
#include <pspkernel.h>
#include <psploadcore.h>
#include <string.h>
#include "../native_control.h"
#include "packet_link.h"
#include "../../virtual_net/driver/usb_transport.h"
PSP_MODULE_INFO("ESP Native WiFi USB",PSP_MODULE_KERNEL,0,1);
static volatile wifi_native_usb_control_t *control;
static SceUID owner=-1,thread=-1;
static unsigned owner_text;
static volatile int running;
static int opened,unsafe;
static int owned(SceModule *m,unsigned p,unsigned n)
{
    if(!m || p<0x08800000u || p>0x0c000000u-n || (p&3))return 0;
    for(unsigned i=0;i<m->nsegment && i<4;++i)
        if(n<=m->segmentsize[i] && p>=m->segmentaddr[i] && p-m->segmentaddr[i]<=m->segmentsize[i]-n)return 1;
    return 0;
}
static int owner_alive(void)
{SceModule *m=sceKernelFindModuleByUID(owner);return m && m->text_addr==owner_text;}
static void publish(unsigned request,int result,const native_scan_session_t *scan)
{
    ++control->revision;__sync_synchronize();
    control->result=result;control->count=scan && !result?scan->count:0;
    memset((void *)control->rows,0,sizeof(control->rows));
    if(scan && !result)memcpy((void *)control->rows,scan->rows,scan->count*sizeof(scan->rows[0]));
    __sync_synchronize();control->completed=request;
    __sync_synchronize();++control->revision;
}
static int close_usb(void)
{
    if(!opened)return 0;
    int r=vnet_usb_stop();if(r<0){unsafe=1;return r;}
    opened=0;unsafe=0;return 0;
}
static int worker(SceSize size,void *args)
{
    (void)size;(void)args;unsigned seen=0,scan_sequence=0;
    unsigned char ignored[PACKET_MAX];
    while(running && owner_alive()) {
        unsigned request=control->request;
        if(request==seen || !request){sceKernelDelayThread(10000);continue;}
        __sync_synchronize();seen=request;
        control->phase=3;control->detail=0;
        unsigned command=control->command;
        native_wifi_profile_t profile=control->profile;
        native_profile_clear((void *)&control->profile,sizeof(control->profile));
        int result=-1,started=0;native_scan_session_t scan;memset(&scan,0,sizeof(scan));
        if(vnet_usb_native_mode(1)<0){native_profile_clear(&profile,sizeof(profile));publish(request,-1,NULL);break;}
        opened=1; /* stop unwinds partial start flags too */
        control->phase=4;int usb_result=vnet_usb_start();control->detail=usb_result;
        if(usb_result<0)control->phase=40+vnet_usb_start_step();
        if(usb_result>=0) {
            control->phase=5;
            unsigned began=sceKernelGetSystemTimeLow();
            while(running && owner_alive() && sceKernelGetSystemTimeLow()-began<20000000u) {
                vnet_usb_poll(ignored,sizeof(ignored));
                if(!vnet_usb_ready()) {
                    /* A new USB epoch discards the previous wire transaction.
                     * Retain the local profile until this request completes. */
                    started=0;control->phase=5;
                    sceKernelDelayThread(1000);continue;
                }
                if(!started) {
                    unsigned transaction=request;
                    if(command==WIFI_NATIVE_SCAN_COMMAND) {
                        if(!++scan_sequence)++scan_sequence;
                        transaction=scan_sequence;
                    }
                    int r=command==WIFI_NATIVE_CONNECT_COMMAND?vnet_usb_profile_begin(request,&profile):
                        command==WIFI_NATIVE_SCAN_COMMAND?vnet_usb_scan_begin(transaction,NATIVE_SCAN_LIMIT):-1;
                    control->phase=6;control->detail=r;
                    if(r<0)break;
                    started=1;
                }
                if(started) {
                    if(command==WIFI_NATIVE_SCAN_COMMAND) {
                        int state=vnet_usb_scan_snapshot(&scan);
                        if(state<0 && scan.error==NATIVE_SCAN_NOT_READY) {
                            /* Boot/radio readiness and USB epoch changes are
                             * transient. Retain the service/power lease and
                             * retry with a fresh wire ID under the same deadline. */
                            started=0;control->phase=5;control->detail=scan.error;
                            sceKernelDelayThread(250000);continue;
                        }
                        if(state){control->phase=7;control->detail=scan.error;result=state>0?0:-(int)scan.error;break;}
                    } else {
                        int applied=vnet_usb_profile_status();unsigned char ip[4],radio[48];
                        control->phase=8;control->detail=applied;
                        if(applied<0)break;
                        if(applied>0 && (vnet_usb_uplink(ip)&1) && vnet_usb_wifi_info(radio) &&
                            !memcmp(radio+8,profile.ssid,32)){result=0;break;}
                    }
                }
                sceKernelDelayThread(1000);
            }
        }
        native_profile_clear(&profile,sizeof(profile));
        if(close_usb()<0)result=-2;
        if(owner_alive())publish(request,result,&scan);
        if(unsafe)break;
    }
    close_usb();return 0;
}
int module_start(SceSize size,void *args)
{
    if(size!=sizeof(wifi_native_usb_args_t) || !args)return -1;
    wifi_native_usb_args_t request;memcpy(&request,args,sizeof(request));
    SceModule *m=sceKernelFindModuleByUID(request.owner);
    unsigned version=sceKernelDevkitVersion();
    if((version!=0x06060010u && version!=0x06060110u) || request.magic!=WIFI_NATIVE_USB_MAGIC ||
        !owned(m,request.address,sizeof(*control)))return -1;
    control=(void *)request.address;
    if(control->magic!=WIFI_NATIVE_USB_MAGIC || control->abi!=WIFI_NATIVE_USB_ABI)return -1;
    control->phase=1;control->detail=0;
    owner=m->modid;owner_text=m->text_addr;
    thread=sceKernelCreateThread("ESPNativeUSB",worker,0x28,0x6000,0,NULL);
    control->phase=2;control->detail=thread;
    if(thread<0)return thread;
    running=1;int r=sceKernelStartThread(thread,0,NULL);
    if(r<0){running=0;sceKernelDeleteThread(thread);thread=-1;}return r;
}
int module_stop(SceSize size,void *args)
{
    (void)size;(void)args;running=0;
    if(thread>=0){unsigned timeout=2000000;if(sceKernelWaitThreadEnd(thread,&timeout)<0)return -2;}
    /* The worker has exited, so it no longer owns the transport. A
     * cancellation timeout may have resolved since its final stop attempt. */
    if(close_usb()<0 || unsafe || opened)return -2;
    if(thread>=0){sceKernelDeleteThread(thread);thread=-1;}return 0;
}
