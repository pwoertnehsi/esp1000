/* Resident native XMB browser and ESP network backend. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspinit.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <pspusb.h>
#include <systemctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pspsysclib.h>
#include "policy.h"
#include "menu_runtime.h"
#include "native_runtime.h"
#include "menu_control.h"
#include "browser_runtime.h"

PSP_MODULE_INFO("ESP WiFi VSH Entry",PSP_MODULE_KERNEL,0,17);
#define CHORD (PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER|PSP_CTRL_SQUARE)
#define ROWS 64u
static SceUID worker=-1;
static volatile int running;
static int (*usb_flag)(void);
static struct {unsigned time;int state,foreign,flag;} history[ROWS];
static unsigned written;
static char diagnostics[4096];
static unsigned diagnostic_size;
static int diagnostic_pending;
static void diagnostic_char(void *unused,int ch)
{
    (void)unused;
    if(ch>=0 && ch<256 && diagnostic_size<sizeof(diagnostics)-1)
        diagnostics[diagnostic_size++]=(char)ch;
}
static void diagnostic(const char *format,...)
{
    if(diagnostic_size>=sizeof(diagnostics)-1)return;
    va_list args;va_start(args,format);
    prnt(diagnostic_char,NULL,format,args);
    va_end(args);
    diagnostics[diagnostic_size]=0;
    diagnostic_pending=1;
}
static void flush_diagnostics(void)
{
    if(!diagnostic_pending)return;
    int state=sceUsbGetState();
    if(state<0 || (state&PSP_USB_ACTIVATED))return;
    /* Finite startup/explicit-shortcut diagnostics only. No writes while a
     * USB profile is active, and no file is held between monitor iterations. */
    SceUID fd=sceIoOpen("ms0:/wifi-vsh.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd<0)return;
    int n=sceIoWrite(fd,diagnostics,diagnostic_size);sceIoClose(fd);
    if(n>0 && (unsigned)n<=diagnostic_size) {
        diagnostic_size-=(unsigned)n;
        memmove(diagnostics,diagnostics+n,diagnostic_size);
    }
    diagnostic_pending=diagnostic_size!=0;
}

static int foreign_usb(void)
{
    static const char *names[]={"USBStor_Driver","USBStorBoot_Driver",
        "USBStorFlash_Driver","USBStorMLN_Driver","USBPSPCommunicationDriver",
        "USBCamDriver","USBCamMicDriver","USBGpsDriver","PSPWifiLink","USBAccBaseDriver"};
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i)
        if(sceUsbGetDrvState(names[i])==1)return (int)i+1;
    return 0;
}
static void resolve_passive_flag(void)
{
    /* Local 6.61 syscon function is four instructions: LUI, cached byte load,
     * JR RA, extract bit4. It issues no syscon command or hardware write.
     * The bit's meaning is not assumed to be accessory-ID detection. */
    SceModule *m=sceKernelFindModuleByName("sceSYSCON_Driver");
    if(!m || m->version[0]!=11 || m->version[1]!=1 || m->text_size!=19980)return;
    unsigned address=sctrlHENFindFunction(m->modname,"sceSyscon_driver",0xDF20C984);
    if(address!=m->text_addr+0x21d0)return;
    const unsigned *p=(const void *)address;
    if((p[0]&0xffff0000u)!=0x3c020000u || (p[1]&0xffff0000u)!=0x90440000u
        || p[2]!=0x03e00008u || p[3]!=0x7c820100u)return;
    unsigned data=((p[0]&0xffffu)<<16)+(int)(short)p[1];
    int owned=0;
    for(unsigned i=0;i<m->nsegment && i<4;++i)
        if(data>=m->segmentaddr[i] && data-m->segmentaddr[i]<m->segmentsize[i])owned=1;
    if(owned)usb_flag=(void *)address;
}
static void remember(unsigned time,int state,int foreign,int flag)
{
    if(written) {
        unsigned previous=(written-1)%ROWS;
        if(history[previous].state==state && history[previous].foreign==foreign && history[previous].flag==flag)return;
    }
    unsigned row=written++%ROWS;
    history[row].time=time;history[row].state=state;
    history[row].foreign=foreign;history[row].flag=flag;
}
static unsigned menu_command;
static int launch(void)
{
    int state=sceUsbGetState();
    if(state<0 || (state&PSP_USB_ACTIVATED) || foreign_usb())return -2;
    if(wifi_browser_status()!=1)return -3;
    return wifi_menu_launch(menu_command?menu_command:WIFI_MENU_BROWSER);
}
static int monitor(SceSize size,void *args)
{
    (void)size;(void)args;wifi_shortcut_t shortcut={0};int previous_chord=0;
    int previous_menu_status=-999;unsigned previous_menu_serial=0;
    int menu_check_started=0,menu_checked=0;unsigned menu_check_time=0;
    int previous_pad_result=-999;unsigned previous_buttons=~0u;
    resolve_passive_flag();
    diagnostic("WORKER_BEGIN syscon=%d\n",usb_flag!=NULL);
    while(running) {
        int state=sceUsbGetState(),foreign=foreign_usb();
        int flag=usb_flag?usb_flag():-1;
        unsigned now=sceKernelGetSystemTimeLow();
        remember(now,state,foreign,flag);
        SceCtrlData pad;memset(&pad,0,sizeof(pad));
        int pad_result=sceCtrlPeekBufferPositive(&pad,1);
        int have_pad=pad_result>0;
        unsigned buttons=pad.Buttons&CHORD;
        if(previous_buttons!=buttons || (pad_result<0 && previous_pad_result!=pad_result))
            diagnostic("INPUT result=%08x buttons=%08x USB=%08x foreign_index=%d vsh=%d\n",
                pad_result,pad.Buttons,state,foreign,sceKernelFindModuleByName("vsh_module")!=NULL);
        previous_buttons=buttons;previous_pad_result=pad_result;
        int chord=have_pad && (pad.Buttons&CHORD)==CHORD;
        int allowed=state>=0 && !(state&PSP_USB_ACTIVATED) && !foreign
            && sceKernelFindModuleByName("vsh_module")!=NULL;
        if(chord && !previous_chord)
            diagnostic("SHORTCUT USB=%08x foreign_index=%d allowed=%d buttons=%08x\n",state,foreign,allowed,pad.Buttons);
        previous_chord=chord;
        static int previous_native_status=-999;
        int native_status=wifi_native_status();
        if(native_status!=previous_native_status){
            unsigned words[4];wifi_native_observed(words);
            diagnostic("NATIVE_BACKEND status=%d detail=%08x stub=%08x/%08x function=%08x api=%08x\n",native_status,wifi_native_detail(),words[0],words[1],words[2],words[3]);
            previous_native_status=native_status;
        }
        static int previous_browser_paths=-999,previous_browser_backend=-999;
        static unsigned previous_consumers=~0u;
        int browser_paths=wifi_browser_status(),browser_backend=wifi_native_browser_status();
        unsigned consumers=wifi_native_browser_consumers();
        if(browser_paths!=previous_browser_paths || browser_backend!=previous_browser_backend || consumers!=previous_consumers) {
            diagnostic("NATIVE_BROWSER paths=%d backend=%d consumers=%x\n",browser_paths,browser_backend,consumers);
            previous_browser_paths=browser_paths;previous_browser_backend=browser_backend;previous_consumers=consumers;
        }
        int menu_status=wifi_menu_status();
        if(menu_status!=previous_menu_status){diagnostic("XMB_MENU status=%d\n",menu_status);previous_menu_status=menu_status;}
        if(menu_status==1 && !menu_check_started){menu_check_started=1;menu_check_time=now;}
        if(menu_check_started && !menu_checked && now-menu_check_time>=5000000u) {
            menu_checked=1;wifi_menu_verify();
        }
        wifi_menu_snapshot_t evidence;wifi_menu_snapshot(&evidence);
        if(evidence.serial!=previous_menu_serial) {
            previous_menu_serial=evidence.serial;
            diagnostic("XMB_VSH_ID result=%d version=%u,%u attr=%04x text=%08x size=%u user_address=%d segments=%u cb=%08x,%08x\n",
                evidence.result,evidence.version[0],evidence.version[1],evidence.attribute,
                evidence.text,evidence.size,evidence.user_address,evidence.segments,
                evidence.bridge_callbacks[0],evidence.bridge_callbacks[1]);
            diagnostic("XMB_VIEW instance=%08x parent=%08x columns=%u\n",evidence.menu_instance,evidence.menu_parent,evidence.category_count);
            for(unsigned i=0;i<8;++i)diagnostic("XMB_COLUMN %u items=%u root_resource=%08x network_resource=%08x\n",i,evidence.item_counts[i],evidence.root_resources[i],evidence.network_resources[i]);
            for(unsigned i=0;i<evidence.segments && i<4;++i)
                diagnostic("XMB_SEGMENT %u address=%08x size=%u\n",i,evidence.segment_address[i],evidence.segment_size[i]);
            for(unsigned i=0;i<evidence.anchor_count && i<16;++i)
                diagnostic("XMB_ANCHOR %u word=%08x\n",i,evidence.anchor_words[i]);
            for(unsigned i=0;i<evidence.row_count && i<16;++i)
                diagnostic("XMB_ROW %u mask=%08x\n",i,evidence.row_masks[i]);
        }
        unsigned activity[3];
        if(wifi_menu_activity(activity))diagnostic("XMB_ACTION calls=%u action=%d arg=%d\n",activity[0],(int)activity[1],(int)activity[2]);
        menu_command=wifi_menu_poll();
        if(menu_command)diagnostic("XMB_DISPATCH command=%u\n",menu_command);
        if(menu_command && !allowed){diagnostic("XMB_MENU_BUSY command=%u USB=%08x foreign=%d\n",menu_command,state,foreign);menu_command=0;}
        int shortcut_launch=wifi_shortcut_update(&shortcut,now,chord,allowed);
        if(shortcut_launch)menu_command=WIFI_MENU_BROWSER;
        if(menu_command || shortcut_launch) {
            int result=launch();diagnostic("NATIVE_BROWSER_REQUEST result=%08x paths=%d\n",result,wifi_browser_status());
        }
        flush_diagnostics();
        sceKernelDelayThread(50000);
    }
    return 0;
}
int module_start(SceSize size,void *args)
{
    (void)size;(void)args;
    unsigned devkit=sceKernelDevkitVersion();int model=sceKernelGetModel(),context=sceKernelInitKeyConfig();
    diagnostic("BEGIN ESP WiFi VSH Entry0.17 devkit=%08x model=%d context=%08x\n",devkit,model,context);
    /* ARK VSH reports 6.60 on this 6.61 installation. This plugin changes no
     * global model values; native menu changes have separate exact ABI guards. */
    if((devkit!=0x06060110 && devkit!=0x06060010) || model!=10 || context!=PSP_INIT_KEYCONFIG_VSH) {
        diagnostic("START_REFUSED\n");flush_diagnostics();return -1;
    }
    wifi_menu_init();
    wifi_native_init();
    int browser_result=wifi_browser_init();diagnostic("NATIVE_BROWSER_PATHS result=%08x status=%d\n",browser_result,wifi_browser_status());
    worker=sceKernelCreateThread("ESPWiFiVshEntry",monitor,0x32,0x4000,0,NULL);
    if(worker<0){wifi_browser_stop();wifi_native_stop();wifi_menu_stop();diagnostic("THREAD_CREATE %08x\n",worker);flush_diagnostics();return worker;}
    running=1;int r=sceKernelStartThread(worker,0,NULL);
    diagnostic("THREAD_START %08x\n",r);
    if(r<0){wifi_browser_stop();wifi_native_stop();wifi_menu_stop();running=0;sceKernelDeleteThread(worker);worker=-1;flush_diagnostics();}
    return r;
}
int module_stop(SceSize size,void *args)
{
    (void)size;(void)args;
    if(!wifi_menu_can_stop())return -2;
    if(wifi_browser_stop()<0)return -2;
    if(wifi_native_stop()<0)return -2;
    if(wifi_menu_stop()<0)return -2;
    running=0;
    if(worker>=0) {
        unsigned timeout=2000000;
        if(sceKernelWaitThreadEnd(worker,&timeout)<0)return -1;
        sceKernelDeleteThread(worker);worker=-1;
    }
    return 0;
}
