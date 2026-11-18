/* Stock netconf scan facade. Only real, completed ESP transactions are listed. */
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspmodulemgr.h>
#include <pspiofilemgr.h>
#include <pspusb.h>
#include <psputility_usbmodules.h>
#include <psputility_netparam.h>
#include <psputility_netmodules.h>
#include <pspnet_apctl.h>
#include "../../common/native_sony_profile.h"
#include <kubridge.h>
#include <string.h>
#include "../native_control.h"
#include "native_adapter.h"
extern int wifiNativeRegister(unsigned,unsigned);
extern int wifiNativeRefresh(void);
static unsigned owned_net_modules;
typedef void (*event_fn)(int,int,int,int,void *);
static wifi_native_hooks_t hooks={.magic=WIFI_NATIVE_HOOK_MAGIC,.abi=3};
static wifi_native_usb_control_t usb={.magic=WIFI_NATIVE_USB_MAGIC,.abi=WIFI_NATIVE_USB_ABI};
static SceUID lock=-1,thread=-1,service=-1;
static int service_started,acc_owned,service_failed;
static SceUID network=-1;
static int network_started,selected_profile;
static unsigned operation=WIFI_NATIVE_SCAN_COMMAND;
static wifi_native_session_control_t session={.magic=WIFI_NATIVE_SESSION_MAGIC,.abi=WIFI_NATIVE_SESSION_ABI};
static volatile int running,pending,cancel;
static int state,complete;
static unsigned serial,count,request;
static native_scan_row_t rows[NATIVE_SCAN_LIMIT];
static unsigned char diagnostic_buffer[257];
static struct {event_fn fn;void *arg;int id,active,busy,owner;} handlers[16];
static void log_result(unsigned command,int result,unsigned row_count)
{
    /* Write only after our USB service stops; refuse mass-storage ownership. */
    if(sceUsbGetState()&0x200)return;
    char line[160];unsigned used=0;
    const char *names[]={"NATIVE_DONE cmd="," result="," rows="," phase="," detail="};
    unsigned values[]={command,(unsigned)result,row_count,session.stage,(unsigned)session.detail};
    static const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<5;++i) {
        unsigned n=strlen(names[i]);memcpy(line+used,names[i],n);used+=n;
        for(unsigned j=0;j<8;++j)line[used++]=hex[(values[i]>>(28-4*j))&15];
    }
    line[used++]='\n';int fd=sceIoOpen("ms0:/native-wifi.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0777);
    if(fd>=0){sceIoWrite(fd,line,used);sceIoClose(fd);}
}
static void enter(void){sceKernelWaitSema(lock,1,NULL);}
static void leave(void){sceKernelSignalSema(lock,1);}
static int gate(void){return running?0:1;}
static int scan(void)
{
    enter();
    if(pending || state){leave();return (int)0x80410a0bu;}
    complete=0;count=0;memset(rows,0,sizeof(rows));cancel=0;operation=WIFI_NATIVE_SCAN_COMMAND;pending=1;
    leave();return 0;
}
static int read_parameter(int profile,int parameter,void *out)
{
    session.stage=200+(unsigned)parameter;
    int r=sceUtilityGetNetParam(profile,parameter,(netData *)out);
    session.detail=r;return r;
}
static int connect(int profile)
{
    if(profile<0 || profile>32)return -1;
    enter();
    if(pending && operation==WIFI_NATIVE_CONNECT_COMMAND && selected_profile==profile && session.alive && !cancel){leave();return 0;}
    if(pending || state){leave();return (int)0x80410a0bu;}
    selected_profile=profile;operation=WIFI_NATIVE_CONNECT_COMMAND;cancel=0;pending=1;
    leave();return 0;
}
static int disconnect(void)
{enter();cancel=1;session.cancel=1;leave();return 0;}
static int get_info(int field,union SceNetApctlInfo *out)
{
    if(!out)return -1;
    enter();int r=-1;
    if(state==4 && network_started && session.ready && session.info_callback) {
        __sync_add_and_fetch(&session.info_users,1);
        __sync_synchronize();
        if(!session.ready || !session.info_callback){__sync_sub_and_fetch(&session.info_users,1);leave();return -1;}
        int (*fn)(int,union SceNetApctlInfo *)=(void *)session.info_callback;r=fn(field,out);
        int parameter=field==0?0:field==4?2:field==13?13:field==14?14:field==15?15:field==17?23:-1;
        if(parameter>=0) {
            netData data;memset(&data,0,sizeof(data));
            if(!sceUtilityGetNetParam(selected_profile,parameter,&data)) {
                if(field==4) {
                    /* Profile/scan AES=4; APCTL GetInfo AES=3. Sony's browser
                     * indexes a four-entry security-label table with this. */
                    static const unsigned types[]={0,1,1,2,3};
                    if(data.asUint>=sizeof(types)/sizeof(types[0])){__sync_sub_and_fetch(&session.info_users,1);leave();return -1;}
                    unsigned value=types[data.asUint];memcpy(out,&value,4);
                }
                else memcpy(out,&data,field==0?64:field==14?128:field==15?2:4);
                r=0;
            }
        }
        __sync_sub_and_fetch(&session.info_users,1);
    }
    leave();return r;
}
static int get_state(int *out)
{if(!out)return (int)0x80410a03u;enter();*out=state;leave();return 0;}
static int get_list(unsigned *size,unsigned *buffer)
{
    if(!size)return (int)0x80410a03u;
    enter();
    if(!complete){leave();return (int)0x80410a09u;}
    unsigned available=*size/8,n=count>20?20:count;
    *size=n*8;
    if(buffer) {
        if(available<n)n=available;
        for(unsigned i=0;i<n;++i){buffer[2*i]=i+1<n?(unsigned)(buffer+2*i+2):0;buffer[2*i+1]=i+1;}
    }
    leave();return 0;
}
static unsigned security(const native_scan_row_t *row)
{
    if(row->auth==NATIVE_AUTH_OPEN)return 0;
    if(row->auth==NATIVE_AUTH_WEP)return 1;
    if(row->auth==NATIVE_AUTH_WPA || row->auth==NATIVE_AUTH_WPA2 || row->auth==NATIVE_AUTH_WPA_MIXED || row->auth==NATIVE_AUTH_WPA3_MIXED) {
        if(row->cipher==NATIVE_CIPHER_AES)return 4;
        if(row->cipher==NATIVE_CIPHER_TKIP)return 2;
    }
    return 3; /* Sony's unsupported marker; never advertise WPA3 as open. */
}
static int get_entry(unsigned id,unsigned field,void *out)
{
    if(!out)return (int)0x80410a03u;
    enter();
    if(!complete || !id || id>count){leave();return (int)0x80410a09u;}
    const native_scan_row_t *row=&rows[id-1];
    switch(field) {
    case 0:memcpy(out,row->bssid,6);break;
    case 1:memset(out,0,33);memcpy(out,row->ssid,row->ssid_length);break;
    case 2:*(unsigned *)out=row->ssid_length;break;
    case 3:*(unsigned char *)out=row->channel;break;
    case 4:{int value=2*((int)row->rssi+100);*(unsigned char *)out=value<0?0:value>100?100:value;break;}
    case 5:*(unsigned *)out=security(row);break;
    default:leave();return (int)0x80410a02u;
    }
    leave();return 0;
}
static int browser_alive(void)
{
    SceModule m;
    return hooks.browser_owners[0]>=0 && kuKernelFindModuleByName("htmlviewer_plugin_module",&m)>=0 && m.modid==hooks.browser_owners[0];
}
static int callback_owner(int id)
{
    if(id==hooks.owner)return 1;
    for(unsigned i=0;i<WIFI_NATIVE_CONSUMERS;++i)if(id==hooks.browser_owners[i] && id>=0)return 1;
    return 0;
}
static int add_handler(event_fn fn,void *arg)
{
    if(!fn)return (int)0x80410a03u;
    SceModule m;unsigned address=(unsigned)fn;
    if(kuKernelFindModuleByAddress((void *)fn,&m)<0 || !callback_owner(m.modid) ||
        address<m.text_addr || address-m.text_addr>=m.text_size || (address&3))return -1;
    enter();int id=-1;
    for(unsigned i=0;i<16;++i)if(!handlers[i].active && !handlers[i].busy) {
        if(++serial>0xffff)serial=1;
        id=0x200000+(serial<<4)+i;
        handlers[i].fn=fn;handlers[i].arg=arg;handlers[i].id=id;handlers[i].owner=m.modid;handlers[i].active=1;break;
    }
    leave();return id;
}
static int del_handler(int id)
{
    unsigned i=(unsigned)id&15;enter();
    if(!handlers[i].active || handlers[i].id!=id){leave();return -1;}
    handlers[i].active=0;
    while(handlers[i].busy && sceKernelGetThreadId()!=thread){leave();sceKernelDelayThread(1000);enter();}
    int any=0;for(unsigned j=0;j<16;++j)any|=handlers[j].active;
    if(!any && !browser_alive())cancel=1;
    leave();return 0;
}
static int log_pointer(void **out)
{if(!out)return -1;*out=diagnostic_buffer;return 0;}
static int log_setting(const char *unused){(void)unused;return 0;}
static void emit(int next,int event,int error)
{
    enter();int old=state;state=next;leave();
    for(unsigned i=0;i<16;++i) {
        enter();event_fn fn=handlers[i].active?handlers[i].fn:NULL;void *arg=handlers[i].arg;
        if(fn) {
            SceModule m;
            if(kuKernelFindModuleByAddress((void *)fn,&m)<0 || m.modid!=handlers[i].owner) {
                handlers[i].active=0;fn=NULL;
            }
        }
        if(fn)handlers[i].busy=1;
        leave();
        if(fn){fn(old,next,event,error,arg);enter();handlers[i].busy=0;leave();}
    }
}
static int stop_service(void)
{
    if(service_started){int status;int r=sceKernelStopModule(service,0,NULL,&status,NULL);if(r<0 || status<0){service_failed=1;return -1;}service_started=0;}
    if(service>=0){if(sceKernelUnloadModule(service)<0){service_failed=1;return -1;}service=-1;}
    service_failed=0;return 0;
}
static int start_service(void)
{
    if(service_failed)return -2;
    if(service>=0)return service_started?0:-1;
    if(!acc_owned) {
        session.stage=101;
        int r=sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC);session.detail=r;
        if(r<0)return r;
        acc_owned=1;
    }
    usb.request=usb.completed=usb.phase=0;usb.result=-1;usb.detail=0;__sync_synchronize();
    session.stage=102;
    service=kuKernelLoadModule("ms0:/seplugins/esp-wifi/wifi_native_usb.prx",0,NULL);
    session.detail=service;if(service<0)return service;
    /* The known kubridge lookup also verifies the owning module. Do not rely
     * on a VSH-context ModuleMgrForUser address-to-ID stub for kernel IPC. */
    session.stage=103;SceModule bridge;
    int lookup=kuKernelFindModuleByAddress((void *)start_service,&bridge);session.detail=lookup;
    if(lookup<0){sceKernelUnloadModule(service);service=-1;return lookup;}
    wifi_native_usb_args_t args={WIFI_NATIVE_USB_MAGIC,(unsigned)&usb,bridge.modid};int status=-1;
    session.stage=104;
    int r=sceKernelStartModule(service,sizeof(args),&args,&status,NULL);
    session.detail=r<0?r:status;
    if(r<0 || status<0){sceKernelUnloadModule(service);service=-1;return r<0?r:status;}
    service_started=1;return 0;
}
static int prepare_network_modules(void)
{
    SceModule m;
    if(kuKernelFindModuleByName("sceNet_Library",&m)<0 || kuKernelFindModuleByName("sceNet_Service",&m)<0) {
        if(sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON)<0)return -1;
        owned_net_modules|=1;
    }
    if(kuKernelFindModuleByName("sceNetInet_Library",&m)<0 || kuKernelFindModuleByName("sceNetResolver_Library",&m)<0) {
        if(sceUtilityLoadNetModule(PSP_NET_MODULE_INET)<0)return -1;
        owned_net_modules|=2;
    }
    return wifiNativeRefresh();
}
static int release_network_modules(void)
{
    /* Browser may still be closing sockets / resolver handles after APCTL
     * disconnect. Keep our utility references until that consumer is gone. */
    if(browser_alive())return 0;
    if(owned_net_modules&2) {
        session.stage=310;log_result(WIFI_NATIVE_CONNECT_COMMAND,0,count);
        if(sceUtilityUnloadNetModule(PSP_NET_MODULE_INET)<0)return -1;
        session.stage=311;log_result(WIFI_NATIVE_CONNECT_COMMAND,0,count);
        owned_net_modules&=~2u;
        if(wifiNativeRefresh()<0)return -1;
    }
    if(owned_net_modules&1) {
        session.stage=312;log_result(WIFI_NATIVE_CONNECT_COMMAND,0,count);
        if(sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON)<0)return -1;
        session.stage=313;log_result(WIFI_NATIVE_CONNECT_COMMAND,0,count);
        owned_net_modules&=~1u;
        if(wifiNativeRefresh()<0)return -1;
    }
    return 0;
}
static int stop_network(void)
{
    enter();session.ready=0;session.cancel=1;leave();
    session.stage=300;log_result(WIFI_NATIVE_CONNECT_COMMAND,0,count);
    if(network_started){int status;int r=sceKernelStopModule(network,0,NULL,&status,NULL);if(r<0 || status<0)return -1;network_started=0;}
    session.stage=301;log_result(WIFI_NATIVE_CONNECT_COMMAND,0,count);
    if(network>=0){if(sceKernelUnloadModule(network)<0)return -1;network=-1;}
    session.stage=302;log_result(WIFI_NATIVE_CONNECT_COMMAND,0,count);
    return release_network_modules();
}
static int start_network(void)
{
    if(network>=0)return -1;
    if(prepare_network_modules()<0)return -1;
    network=kuKernelLoadModule("ms0:/seplugins/esp-wifi/wifi_native_session.prx",0,NULL);
    if(network<0)return -1;
    session.cancel=0;session.ready=0;session.result=-1;session.info_callback=0;
    session.alive=session.recovering=0;session.recoveries=session.info_users=0;
    unsigned address=(unsigned)&session;int status;
    int r=sceKernelStartModule(network,sizeof(address),&address,&status,NULL);
    if(r<0 || status<0){sceKernelUnloadModule(network);network=-1;return -1;}
    network_started=1;
    unsigned began=sceKernelGetSystemTimeLow();
    while(running && !cancel && !session.ready && sceKernelGetSystemTimeLow()-began<35000000u)sceKernelDelayThread(10000);
    return session.ready && !cancel?0:-1;
}
static int monitor_network(void)
{
    while(running && !cancel && session.alive) {
        if(!session.ready && state==4)emit(2,10,0);
        else if(session.ready && state!=4){emit(3,3,0);emit(4,4,0);}
        SceModule m;
        int netconf=kuKernelFindModuleByName("sceVshNetconf_Module",&m)>=0 && m.modid==hooks.owner;
        if(!netconf && !browser_alive()){cancel=1;session.cancel=1;break;}
        sceKernelDelayThread(20000);
    }
    return !cancel && !session.alive?-1:0;
}
static int worker(SceSize size,void *args)
{
    (void)size;(void)args;
    while(running) {
        if(!pending){sceKernelDelayThread(10000);continue;}
        unsigned command=operation;
        emit(command==WIFI_NATIVE_SCAN_COMMAND?1:2,command==WIFI_NATIVE_SCAN_COMMAND?1:0,0);
        native_wifi_profile_t profile;memset(&profile,0,sizeof(profile));
        int result=0;
        if(command==WIFI_NATIVE_CONNECT_COMMAND)
            result=native_sony_profile_read(selected_profile,read_parameter,&profile);
        if(!result)result=start_service();
        if(!result) {
            if(!++request)++request;
            usb.command=command;
            if(command==WIFI_NATIVE_CONNECT_COMMAND)usb.profile=profile;
            session.stage=105;__sync_synchronize();usb.request=request;
            native_profile_clear(&profile,sizeof(profile));
            unsigned began=sceKernelGetSystemTimeLow();
            while(running && !cancel && usb.completed!=request && sceKernelGetSystemTimeLow()-began<22000000u)sceKernelDelayThread(10000);
            session.stage=1000+usb.phase;session.detail=cancel?-2:usb.detail;
            if(usb.completed==request && !cancel) {
                __sync_synchronize();result=usb.result;
                if(command==WIFI_NATIVE_SCAN_COMMAND) {
                    if(!result) {
                        if(usb.count>NATIVE_SCAN_LIMIT){result=-(int)NATIVE_SCAN_BAD_WIRE;session.detail=NATIVE_SCAN_BAD_WIRE;}
                        else {enter();count=usb.count;memcpy(rows,usb.rows,count*sizeof(rows[0]));complete=1;leave();}
                    }
                }
            } else result=-1;
        }
        native_profile_clear(&profile,sizeof(profile));
        if(stop_service()<0 && !result)result=-2;
        int disconnected=0;
        if(!result && command==WIFI_NATIVE_CONNECT_COMMAND) {
            result=start_network();
            if(!result) {
                emit(3,3,0);emit(4,4,0);
                result=monitor_network();
            }
            /* Publish disconnect before native NET teardown. Sony callers
             * must not wait for GOT_IP to clear while we wait for their NET
             * resources to be released. */
            emit(0,result && !cancel?6:5,result && !cancel?(int)0x80410a0du:0);
            disconnected=1;
            if(stop_network()<0)result=-1;
        }
        log_result(command,result,count);
        enter();pending=0;leave();
        if(!disconnected)emit(0,result && !cancel?6:command==WIFI_NATIVE_SCAN_COMMAND?2:5,
            result && !cancel?(int)0x80410a0du:0);
    }
    stop_service();return 0;
}
int wifi_native_adapter_start(void)
{
    lock=sceKernelCreateSema("ESPNativeLock",0,1,1,NULL);if(lock<0)return -1;
    thread=sceKernelCreateThread("ESPNativeSettings",worker,0x30,0x6000,PSP_THREAD_ATTR_USER,NULL);
    if(thread<0){sceKernelDeleteSema(lock);lock=-1;return -1;}
    running=1;int r=sceKernelStartThread(thread,0,NULL);
    if(r<0){running=0;sceKernelDeleteThread(thread);thread=-1;sceKernelDeleteSema(lock);lock=-1;return r;}
    unsigned callbacks[]={ (unsigned)gate,(unsigned)scan,(unsigned)get_list,(unsigned)get_entry,
        (unsigned)add_handler,(unsigned)del_handler,(unsigned)get_state,(unsigned)log_pointer,(unsigned)log_setting,
        (unsigned)connect,(unsigned)disconnect,(unsigned)get_info};
    memcpy(hooks.callbacks,callbacks,sizeof(callbacks));
    r=wifiNativeRegister((unsigned)&hooks,sizeof(hooks));
    if(r<0)wifi_native_adapter_stop();
    else wifiNativeRefresh();
    return r;
}
int wifi_native_adapter_stop(void)
{
    running=0;cancel=1;
    if(thread>=0){unsigned timeout=3000000;if(sceKernelWaitThreadEnd(thread,&timeout)<0)return -2;}
    if(stop_service()<0 || stop_network()<0)return -2;
    if(thread>=0){sceKernelDeleteThread(thread);thread=-1;}
    if(acc_owned){if(sceUtilityUnloadUsbModule(PSP_USB_MODULE_ACC)<0)return -2;acc_owned=0;}
    if(lock>=0){sceKernelDeleteSema(lock);lock=-1;}return 0;
}
