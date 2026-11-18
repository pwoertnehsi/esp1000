#include <pspkernel.h>
#include <psploadcore.h>
#include <psputility_usbmodules.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <psputility.h>
#include <psputility_htmlviewer.h>
#include <psputility_netmodules.h>
#include <psputility_netparam.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <pspnet_apctl.h>
#include <psphttp.h>
#include <pspssl.h>
#include <kubridge.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../../common/link_protocol.h"
#include "../control.h"
#include "assets.h"
#include "adapter.h"
#include "dns.h"
#include "lookup.h"
#include "browser_state.h"
#include "native_dialog.h"
#include "../../wifi_plugin/menu_control.h"
#include "../../virtual_net/app/native_http.h"
PSP_MODULE_INFO("Stock Browser Test",0,0,12);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(1024);
#define BROWSER_MEMORY (10*1024*1024)
static volatile int exiting,done,job_result;
static int job,ready,viewer,gu,launched,shutdown_requested,cleanup_failed;
static SceUID thread=-1,helper=-1,vpl=-1;
static int helper_started,service_started,usb_owned,adapter_live,net,inet,resolver,apctl,ssl,http,https;
static unsigned groups;
static SceUID service=-1;
static volatile vnet_start_result_t transport;
static char service_path[512];
static int online=1;
static int native_ui;
static unsigned menu_entry;
static int close_requested;
static const int module_ids[]={PSP_NET_MODULE_COMMON,PSP_NET_MODULE_INET,PSP_NET_MODULE_PARSEURI,PSP_NET_MODULE_PARSEHTTP,PSP_NET_MODULE_HTTP,PSP_NET_MODULE_SSL};
static volatile browser_status_t helper_status;
static pspUtilityHtmlViewerParam params;
static char helper_path[512],offline_url[512];
static char draw_list[65536] __attribute__((aligned(64)));
static void say(const char *format,...)
{
    char text[320];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    if(!gu && !native_ui)pspDebugScreenPrintf("%s",text);
    SceUID fd=sceIoOpen("ms0:/browser-test.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd>=0){sceIoWrite(fd,text,strlen(text));sceIoClose(fd);sceIoSync("ms0:",0);}
}
static int exit_callback(int a,int b,void *c){(void)a;(void)b;(void)c;exiting=1;return 0;}
static void profile_inventory(void)
{
    unsigned found=0;
    for(int id=1;id<=32 && !exiting;++id) {
        if(sceUtilityCheckNetParam(id)<0)continue;
        netData value;memset(&value,0,sizeof(value));
        int r=sceUtilityGetNetParam(id,PSP_NETPARAM_SECURE,&value);
        say("PROFILE index=%d security_result=%08x security=%u\n",id,r,r<0?0:value.asUint);++found;
    }
    say("PROFILE_COUNT=%u (slots1..32; no keys read)\n",found);
}
static int check_assets(void)
{
    unsigned char buffer[4096];
    for(unsigned i=0;i<sizeof(browser_assets)/sizeof(*browser_assets);++i) {
        SceUID fd=sceIoOpen(browser_assets[i].path,PSP_O_RDONLY,0);
        if(fd<0){say("ASSET_OPEN %s result=%08x\n",browser_assets[i].path,fd);return -1;}
        unsigned size=0,crc=~0u;int n;
        while((n=sceIoRead(fd,buffer,sizeof(buffer)))>0) {
            if(exiting || (unsigned)n>browser_assets[i].bytes-size){sceIoClose(fd);return -1;}
            size+=n;crc=link_crc_update(crc,buffer,n);
        }
        int close=sceIoClose(fd);
        if(n<0 || close<0 || size!=browser_assets[i].bytes || (crc^~0u)!=browser_assets[i].crc) {
            say("ASSET_REFUSED %s bytes=%u crc=%08x\n",browser_assets[i].path,size,crc^~0u);return -1;
        }
        say("ASSET_OK %s bytes=%u\n",browser_assets[i].path,size);
    }
    return 0;
}
static int adapter_report(const char *phase,int result)
{say("ADAPTER %s: %08x\n",phase,result);return exiting?-1:0;}
static int lookup_cancelled(void)
{
    unsigned required=VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY;
    return exiting || (transport.flags&required)!=required;
}
static int initialize_adapter(void)
{
    int r=sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC);say("USB_ACC_LOAD %08x\n",r);
    if(r!=0)return -1;
    usb_owned=1;
    service=kuKernelLoadModule(service_path,0,NULL);say("VNET_LOAD %08x\n",service);if(service<0)return -1;
    vnet_start_args_t args={VNET_START_MAGIC,(unsigned)&transport};int status=0;
    r=sceKernelStartModule(service,sizeof(args),&args,&status,NULL);
    say("VNET_START api=%08x status=%08x result=%08x\n",r,status,transport.result);
    if(r<0 || status<0)return -1;
    service_started=1;
    if(transport.result!=0)return -1;
    unsigned deadline=sceKernelGetSystemTimeLow()+30000000u,last=~0u;
    while(!exiting && (int)(deadline-sceKernelGetSystemTimeLow())>0) {
        unsigned flags=transport.flags;
        if(flags!=last){last=flags;say("ADAPTER_WAIT USB=%u WiFi=%u info=%u\n",!!(flags&VNET_USB_READY),!!(flags&VNET_UPLINK_READY),!!(flags&VNET_WIFI_INFO_READY));}
        if((flags&(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY))==(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY))break;
        sceKernelDelayThread(100000);
    }
    if(exiting || (transport.flags&(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY))!=(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY))return -1;
    if(vnet_run_local_http(adapter_report)!=0)return -1;
    say("UNBOUND_TCP_BEGIN\n");r=vnet_run_unbound_local_http(adapter_report);say("UNBOUND_TCP_END %08x\n",r);if(r || exiting)return -1;
    if(vnet_install_default_route(adapter_report)!=0)return -1;
    vnet_start_result_t snapshot;if(!vnet_status_snapshot(&snapshot,&transport))return -1;
    r=browser_dns_configure(snapshot.wifi_info,snapshot.wifi_info+4);say("DNS_CONFIGURE %08x\n",r);if(r)return -1;
    if(browser_lookup_run(adapter_report,lookup_cancelled)!=0)return -1;
    r=adapter_start(&transport);say("ADAPTER_SESSION_START %08x\n",r);if(r)return -1;
    adapter_live=1;return 0;
}
static int initialize(void)
{
    profile_inventory();
    if(exiting || check_assets()<0)return -1;
    int r;
    for(unsigned i=0;i<6;++i) {
        say("LOAD_NET_BEGIN id=%d\n",module_ids[i]);r=sceUtilityLoadNetModule(module_ids[i]);
        say("LOAD_NET_END id=%d result=%08x\n",module_ids[i],r);
        if(r!=0)return -1;
        groups|=1u<<i;
        if(exiting)return -1;
    }
#define INIT(label,expr,flag) do {say(label "_BEGIN\n");r=(expr);say(label "_END %08x\n",r);if(r!=0){return -1;}flag=1;if(exiting){return -1;}}while(0)
    INIT("NET_INIT",sceNetInit(128*1024,42,4096,42,4096),net);
    INIT("INET_INIT",sceNetInetInit(),inet);
    INIT("RESOLVER_INIT",sceNetResolverInit(),resolver);
    INIT("APCTL_INIT",sceNetApctlInit(0x2000,0x30),apctl);
    int state=-1;r=sceNetApctlGetState(&state);say("APCTL_STATE result=%08x state=%d (no Connect requested)\n",r,state);
    if(r!=0 || state!=PSP_NET_APCTL_STATE_DISCONNECTED)return -1;
    INIT("SSL_INIT",sceSslInit(0x28000),ssl);
    INIT("HTTP_INIT",sceHttpInit(0x25800),http);
    INIT("HTTPS_INIT",sceHttpsInit(0,0,0,0),https);
#undef INIT
    if(initialize_adapter()!=0)return -1;
    helper=kuKernelLoadModule(helper_path,0,NULL);say("HELPER_LOAD %08x\n",helper);
    if(helper<0)return -1;
    SceModule app;
    if(kuKernelFindModuleByName("Stock Browser Test",&app)<0)return -1;
    browser_start_t request={.magic=BROWSER_START_MAGIC,.result_address=(unsigned)&helper_status,.callback_module=app.modid,
        .callbacks={(unsigned)adapter_get_state,(unsigned)adapter_get_info,(unsigned)adapter_connect,(unsigned)adapter_disconnect,
                    (unsigned)adapter_add_handler,(unsigned)adapter_del_handler}};int status=0;
    r=sceKernelStartModule(helper,sizeof(request),&request,&status,NULL);
    say("HELPER_START api=%08x status=%08x result=%08x patches=%u\n",r,status,helper_status.result,helper_status.patches);
    if(r<0 || status<0)return -1;
    helper_started=1;
    if(helper_status.result!=0 || !helper_status.active || helper_status.apctl_hooks!=6)return -1;
    int adapter_state=-1;r=sceNetApctlGetState(&adapter_state);
    say("PUBLIC_APCTL_STATE result=%08x state=%d hooks=%u\n",r,adapter_state,helper_status.apctl_hooks);
    if(r || adapter_state!=PSP_NET_APCTL_STATE_GOT_IP)return -1;
    return 0;
}
static int begin_viewer(void)
{
    if(exiting)return -1;
    vpl=sceKernelCreateVpl("BrowserLocalVpl",PSP_MEMORY_PARTITION_USER,0,BROWSER_MEMORY+256,NULL);
    say("VIEWER_VPL %08x\n",vpl);if(vpl<0)return -1;
    memset(&params,0,sizeof(params));params.base.size=sizeof(params);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,&params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP,&params.base.buttonSwap);
    params.base.graphicsThread=17;params.base.accessThread=19;params.base.fontThread=18;params.base.soundThread=16;
    params.memsize=BROWSER_MEMORY;
    int r=sceKernelAllocateVpl(vpl,params.memsize,&params.memaddr,NULL);say("VIEWER_ALLOC %08x\n",r);if(r<0)return -1;
    params.initialurl=online==2?"http://192.168.77.1:8080/":online?(menu_entry==WIFI_MENU_MANUAL?"http://manuals.playstation.net/document/en/psp/current/":"http://suckless.org/faq/"):offline_url;params.homeurl=params.initialurl;params.numtabs=1;
    params.interfacemode=PSP_UTILITY_HTMLVIEWER_INTERFACEMODE_FULL;
    params.options=PSP_UTILITY_HTMLVIEWER_DISABLE_STARTUP_LIMITS|PSP_UTILITY_HTMLVIEWER_DISABLE_EXIT_DIALOG;
    params.cookiemode=PSP_UTILITY_HTMLVIEWER_COOKIEMODE_DISABLED;
    params.textsize=PSP_UTILITY_HTMLVIEWER_TEXTSIZE_NORMAL;params.displaymode=PSP_UTILITY_HTMLVIEWER_DISPLAYMODE_SMART_FIT;
    params.connectmode=PSP_UTILITY_HTMLVIEWER_CONNECTMODE_LAST;
    params.disconnectmode=PSP_UTILITY_HTMLVIEWER_DISCONNECTMODE_DISABLE;
    params.dldirname="/PSP/COMMON";
    say("VIEWER_INIT_BEGIN url=%s size=%u\n",params.initialurl,(unsigned)sizeof(params));
    if(exiting)return -1;
    r=sceUtilityHtmlViewerInitStart(&params);say("VIEWER_INIT_END %08x\n",r);
    return r;
}
static int cleanup(void)
{
    if(viewer)return -1;
    int lookup_cleanup=browser_lookup_cleanup();say("DNS_WORKER_CLEANUP %08x\n",lookup_cleanup);
    if(lookup_cleanup)return -1;
    if(adapter_live && adapter_pause()<0)return -1;
    if(helper_started) {
        int status=0,r=sceKernelStopModule(helper,0,NULL,&status,NULL);
        say("HELPER_STOP api=%08x status=%08x restore_fail=%u\n",r,status,helper_status.restore_fail);
        if(r<0 || status!=0)return -1;
        helper_started=0;
    }
    if(helper>=0){int r=sceKernelUnloadModule(helper);say("HELPER_UNLOAD %08x\n",r);if(r<0)return -1;helper=-1;}
    if(adapter_live){adapter_dispose();adapter_live=0;}
    if(vpl>=0) {
        if(params.memaddr){if(sceKernelFreeVpl(vpl,params.memaddr)<0)return -1;params.memaddr=NULL;}
        if(sceKernelDeleteVpl(vpl)<0)return -1;
        vpl=-1;
    }
#define TERM(label,expr,flag) do {if(flag){int r=(expr);say(label " %08x\n",r);if(r!=0){return -1;}flag=0;}}while(0)
    TERM("HTTPS_TERM",sceHttpsEnd(),https);TERM("HTTP_TERM",sceHttpEnd(),http);TERM("SSL_TERM",sceSslEnd(),ssl);
    if(service_started) {
        int status=0,r=sceKernelStopModule(service,0,NULL,&status,NULL);
        say("VNET_STOP api=%08x status=%08x\n",r,status);if(r<0 || status!=0)return -1;service_started=0;
    }
    if(service>=0){int r=sceKernelUnloadModule(service);say("VNET_UNLOAD %08x\n",r);if(r<0)return -1;service=-1;}
    int dns_result=browser_dns_restore();say("DNS_RESTORE %08x\n",dns_result);if(dns_result)return -1;
    if(usb_owned){int r=sceUtilityUnloadUsbModule(PSP_USB_MODULE_ACC);say("USB_ACC_UNLOAD %08x\n",r);if(r)return -1;usb_owned=0;}
    TERM("APCTL_TERM",sceNetApctlTerm(),apctl);TERM("RESOLVER_TERM",sceNetResolverTerm(),resolver);
    TERM("INET_TERM",sceNetInetTerm(),inet);TERM("NET_TERM",sceNetTerm(),net);
#undef TERM
    for(int i=5;i>=0;--i)if(groups&(1u<<i)) {
        int r=sceUtilityUnloadNetModule(module_ids[i]);say("UNLOAD_NET id=%d result=%08x\n",module_ids[i],r);
        if(r!=0)return -1;
        groups&=~(1u<<i);
    }
    return 0;
}
static int run_job(SceSize size,void *args)
{
    (void)size;(void)args;
    int r=job==1?initialize():job==2?begin_viewer():job==3?sceUtilityHtmlViewerShutdownStart():job==5?browser_request_close():cleanup();
    if(net)sceNetFreeThreadinfo(sceKernelGetThreadId());
    job_result=r;__sync_synchronize();done=1;return 0;
}
static unsigned job_time;
static int start_job(int kind)
{
    job=kind;done=0;job_time=sceKernelGetSystemTimeLow();
    thread=sceKernelCreateThread("BrowserTestJob",run_job,0x28,0x10000,PSP_THREAD_ATTR_USER,NULL);
    if(thread<0)return -1;
    int r=sceKernelStartThread(thread,0,NULL);
    if(r<0){sceKernelDeleteThread(thread);thread=-1;}return r;
}
static void setup_gu(void)
{
    sceGuInit();sceGuStart(GU_DIRECT,draw_list);
    sceGuDrawBuffer(GU_PSM_8888,(void *)0,512);sceGuDispBuffer(480,272,(void *)0x88000,512);
    sceGuDepthBuffer((void *)0x110000,512);sceGuOffset(0,0);sceGuScissor(0,0,480,272);sceGuEnable(GU_SCISSOR_TEST);
    sceGuClearColor(0);sceGuClearDepth(0);sceGuClearStencil(0);sceGuFinish();sceGuSync(0,0);
    sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);gu=1;
}
static void finish_gu(void){if(gu){sceGuTerm();gu=0;if(!native_ui)pspDebugScreenInit();}}
void browser_ui_begin(void){if(!gu)setup_gu();}
void browser_ui_frame_begin(void)
{sceGuStart(GU_DIRECT,draw_list);sceGuClear(GU_COLOR_BUFFER_BIT|GU_STENCIL_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);sceGuFinish();sceGuSync(0,0);}
void browser_ui_frame_end(void)
{sceDisplayWaitVblankStart();sceGuSwapBuffers();sceKernelDelayThreadCB(20000);}
void browser_ui_end(void){finish_gu();}
static int automatic_entry;
int main(int argc,char **argv)
{
    if(argc>2 && argv[2] && !strncmp(argv[2],"--wifi-menu=",12)) {
        unsigned value=0;char extra;
        if(sscanf(argv[2]+12,"%u%c",&value,&extra)==1 && value>0 && value<WIFI_MENU_COUNT)menu_entry=value;
    }
    native_ui=menu_entry!=0;
    if(!native_ui)pspDebugScreenInit();
    say("ESP Browser Test v0.12\nCROSS: connect and verify preconfigured ESP adapter\nSQUARE: internet; TRIANGLE: file; START: ESP HTTP\nCIRCLE/HOME: exit (close browser first)\nConnect ESP and enable its hotspot. No flash/profile writes.\n");
    if(argc>1 && argv[1] && !strncmp(argv[1],"--wifi-entry\n",13)) {
        unsigned n=strnlen(argv[1],3072);
        if(n<3072)for(unsigned offset=0;offset<n;offset+=250)
            say("%.*s",(int)(n-offset>250?250:n-offset),argv[1]+offset);
    }
    SceUID cb=sceKernelCreateCallback("BrowserTestExit",exit_callback,NULL);if(cb<0 || sceKernelRegisterExitCallback(cb)<0)return -1;
    if(argc<1 || !argv[0] || strncmp(argv[0],"ms0:",4) || strlen(argv[0])>440)return -1;
    strcpy(helper_path,argv[0]);char *slash=strrchr(helper_path,'/');if(!slash)return -1;
    strcpy(slash+1,"browser_paths.prx");
    strcpy(service_path,helper_path);strcpy(strrchr(service_path,'/')+1,"psp_vnet.prx");
    snprintf(offline_url,sizeof(offline_url),"file:%.*s/offline.html",(int)(slash-helper_path)-4,helper_path+4);
    if(menu_entry) {
        say("XMB_MENU_ENTRY command=%u\n",menu_entry);
        automatic_entry=wifi_menu_uses_launcher(menu_entry);
        if(!automatic_entry){sceKernelExitGame();return 0;}
        
    }
    unsigned previous_buttons=0,last_stats=~0u,last_asset_events=0;int initial_tested=0,last_status=-1,stall=0,shutdown_attempted_status=-1;
    for(;;) {
        SceCtrlData pad;sceCtrlPeekBufferPositive(&pad,1);unsigned pressed=pad.Buttons&~previous_buttons;previous_buttons=pad.Buttons;
        if(!viewer && (pressed&PSP_CTRL_CIRCLE))exiting=1;
        if(thread>=0 && done) {
            sceKernelWaitThreadEnd(thread,NULL);sceKernelDeleteThread(thread);thread=-1;stall=0;
            say("JOB_END kind=%d result=%08x\n",job,job_result);
            if(job==1){ready=job_result==0;say("Initialization %s. %s\n",ready?"passed":"failed",ready?"SQUARE: internet; TRIANGLE: file; START: ESP HTTP.":"CIRCLE exits; retain the log.");}
            if(job==1 && native_ui && !ready && !exiting) {
                int dialog=browser_native_dialog("The network connection could not be established.",&exiting,NULL);
                say("NATIVE_ERROR_DIALOG %08x\n",dialog);if(dialog==-2)cleanup_failed=1;exiting=1;
            }
            if(job==2){viewer=job_result>=0;if(!viewer){finish_gu();say("Viewer init failed: %08x\n",job_result);
                if(native_ui){int dialog=browser_native_dialog("The Internet Browser could not be started.",&exiting,NULL);if(dialog==-2)cleanup_failed=1;exiting=1;}
            }}
            if(job==5){close_requested=1;say("BROWSER_CLOSE_REQUEST %08x\n",job_result);}
            if(job==3){shutdown_requested=job_result>=0;say("VIEWER_SHUTDOWN_REQUEST %08x\n",job_result);}
            if(job==4){if(!job_result){say("END: returning to XMB\n");sceKernelExitGame();return 0;}cleanup_failed=1;say("Cleanup could not restore/release resources. Restart required.\n");}
        }
        unsigned stats=helper_status.patches+helper_status.refused+helper_status.module_mask;
        if(helper_started && stats!=last_stats){last_stats=stats;say("PATH_STATUS active=%u patches=%u observed=%u refused=%u modules=%02x gates=%u last=%08x\n",helper_status.active,helper_status.patches,helper_status.observed,helper_status.refused,helper_status.module_mask,helper_status.browser_gates,helper_status.last_result);}
        if(helper_started && helper_status.asset_events!=last_asset_events) {
            last_asset_events=helper_status.asset_events;
            say("ASSET_DEVICE events=%u operation=%08x result=%08x\n",last_asset_events,helper_status.asset_command,helper_status.asset_result);
        }
        if(thread>=0 && !done && !stall && sceKernelGetSystemTimeLow()-job_time>10000000u) {
            stall=1;say("JOB_STALLED kind=%d: controls active; resources retained; restart if call cannot return.\n",job);
        }
        if(gu){sceGuStart(GU_DIRECT,draw_list);sceGuClear(GU_COLOR_BUFFER_BIT|GU_STENCIL_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);sceGuFinish();sceGuSync(0,0);}
        if(viewer) {
            int status=sceUtilityHtmlViewerGetStatus();
            if(status!=last_status){last_status=status;say("VIEWER_STATUS=%d result=%08x memused=%u\n",status,params.base.result,params.memused);}
            browser_log_state(say);
            if(status==PSP_UTILITY_DIALOG_VISIBLE)sceUtilityHtmlViewerUpdate(1);
            if(status==PSP_UTILITY_DIALOG_NONE){if(automatic_entry)exiting=1;viewer=0;finish_gu();say("Browser closed. CIRCLE exits.\n");}
            else if(thread<0 && !shutdown_requested && status!=shutdown_attempted_status && (status==PSP_UTILITY_DIALOG_QUIT)) {
                shutdown_attempted_status=status;
                if(start_job(3)<0){cleanup_failed=1;say("Cannot request browser shutdown. Restart required.\n");}
            } else if(thread<0 && exiting && !close_requested && status==PSP_UTILITY_DIALOG_VISIBLE) {
                if(start_job(5)<0){close_requested=1;say("Cannot request normal browser close.\n");}
            }
        }
        if(thread<0 && !viewer && !cleanup_failed) {
            if(exiting){finish_gu();if(start_job(4)<0)cleanup_failed=1;}
            else if(((pressed&PSP_CTRL_CROSS) || automatic_entry) && !initial_tested){initial_tested=1;
                if(start_job(1)<0) {
                    cleanup_failed=1;
                    if(native_ui) {
                        int dialog=browser_native_dialog("The Internet Browser could not be started.",&exiting,NULL);
                        if(dialog!=-2){sceKernelExitGame();return 0;}
                    }
                }
                else if(native_ui) {
                    int dialog=browser_native_dialog("Connecting...",&exiting,&done);
                    say("NATIVE_CONNECT_DIALOG %08x\n",dialog);if(dialog==-2)cleanup_failed=1;
                }}
            else if(((pressed&(PSP_CTRL_SQUARE|PSP_CTRL_TRIANGLE|PSP_CTRL_START)) || automatic_entry==1) && ready && !launched){online=automatic_entry==1?1:pressed&PSP_CTRL_START?2:!!(pressed&PSP_CTRL_SQUARE);launched=1;setup_gu();if(start_job(2)<0){finish_gu();cleanup_failed=1;}}
        }
        if(gu){sceDisplayWaitVblankStart();sceGuSwapBuffers();}
        sceKernelDelayThreadCB(20000);
    }
}
