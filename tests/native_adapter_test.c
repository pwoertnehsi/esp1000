#include <assert.h>
#include <stdio.h>
#include "native_adapter_shim.h"
#include "../wifi_plugin/menu_bridge/native_adapter.c"
static char last_log[160];
static unsigned facade_traces;
int sceUsbGetState(void){return 0;}
int sceUsbGetDrvState(const char *p){assert(!strcmp(p,"USBStor_Driver"));return 2;}
int sceIoOpen(const char *p,int f,int m){(void)p;(void)f;(void)m;return 7;}
int sceIoWrite(int f,const void *p,unsigned n){assert(f==7 && n<sizeof(last_log));if(n>=7 && !memcmp(p,"FACADE ",7)){++facade_traces;return n;}memcpy(last_log,p,n);last_log[n]=0;return n;}
int sceIoClose(int f){(void)f;return 0;}
static int lookup_result,browser_available,profile_security=-1;
int kuKernelFindModuleByAddress(void *p,SceModule *m){if(lookup_result)return lookup_result;m->modid=hooks.owner;m->text_addr=(unsigned)(uintptr_t)p;m->text_size=4;m->gp_value=0x09123450;return 0;}
int kuKernelFindModuleByName(char *p,SceModule *m){
    if(!strcmp(p,"htmlviewer_plugin_module")){if(!browser_available)return -1;m->modid=123;return 0;}
    m->modid=hooks.owner;return 0;
}
static unsigned callback_invocations;
void wifiNativeInvoke(unsigned gp,event_fn fn,int old,int next,int code,int error,void *arg)
{assert(gp==0x09123450);++callback_invocations;fn(old,next,code,error,arg);}
static unsigned clock_us;
static int utility_unloads;
static int simulate_reconnect,reconnect_ticks;
static int loader_result=-55,start_result,start_status,reply_error,stop_error;
static int events_seen[8],event_count;
int sceKernelWaitSema(int a,int b,void *c){(void)a;(void)b;(void)c;return 0;}
int sceKernelSignalSema(int a,int b){(void)a;(void)b;return 0;}
int sceKernelGetThreadId(void){return thread;}
unsigned sceKernelGetSystemTimeLow(void){return clock_us;}
int sceKernelDelayThread(unsigned n){
    clock_us+=n;
    if(simulate_reconnect) {
        if(++reconnect_ticks==1){session.ready=0;session.recovering=1;}
        else if(reconnect_ticks==2){assert(state==2 && network_started);session.ready=1;session.recovering=0;}
        else {assert(state==4 && network_started);cancel=1;}
        return 0;
    }
    if(reply_error && service_started && usb.request){usb.result=reply_error;usb.phase=7;usb.detail=-reply_error;usb.completed=usb.request;}
    if(!pending)running=0;
    return 0;
}
int sceKernelCreateSema(const char *n,int a,int b,int c,void *d){(void)n;(void)a;(void)b;(void)c;(void)d;return 5;}
int sceKernelCreateThread(const char *n,int (*fn)(SceSize,void *),int a,int b,int c,void *d){(void)n;(void)fn;(void)a;(void)b;(void)c;(void)d;return 7;}
int sceKernelStartThread(int a,unsigned b,void *c){(void)a;(void)b;(void)c;return 0;}
int sceKernelDeleteThread(int a){(void)a;return 0;}int sceKernelWaitThreadEnd(int a,unsigned *b){(void)a;(void)b;return 0;}
int sceKernelDeleteSema(int a){(void)a;return 0;}
int sceKernelStartModule(int a,unsigned b,void *c,int *d,void *e){
    (void)e;assert(a==10 && b==sizeof(wifi_native_usb_args_t));
    wifi_native_usb_args_t *args=c;assert(args->owner==(unsigned)hooks.owner && args->address==(unsigned)(uintptr_t)&usb);
    *d=start_status;return start_result;
}
int sceKernelStopModule(int a,unsigned b,void *c,int *d,void *e){(void)a;(void)b;(void)c;(void)e;*d=stop_error;return 0;}
int sceKernelUnloadModule(int a){(void)a;return 0;}
int sceKernelGetModuleIdByAddress(const void *p){(void)p;assert(0);return -1;}
int sceUtilityLoadUsbModule(int a){(void)a;return 0;}int sceUtilityUnloadUsbModule(int a){(void)a;return 0;}
int sceUtilityGetNetParam(int a,int b,netData *c){(void)a;if(b==2 && profile_security>=0){c->asUint=profile_security;return 0;}return -1;}
int kuKernelLoadModule(const char *a,int b,void *c){(void)a;(void)b;(void)c;return loader_result;}
int wifiNativeRefresh(void){return 0;}
int sceUtilityLoadNetModule(int n){(void)n;return 0;}
int sceUtilityUnloadNetModule(int n){(void)n;assert(!browser_alive());++utility_unloads;return 0;}
int wifiNativeRegister(unsigned a,unsigned b){assert(a && b==sizeof(hooks));for(unsigned i=0;i<WIFI_NATIVE_CONSUMERS;++i)hooks.browser_owners[i]=-1;return 0;}
static void __attribute__((aligned(4))) event(int old,int next,int code,int error,void *arg)
{(void)old;(void)next;(void)error;assert(arg==(void *)1);assert(event_count<8);events_seen[event_count++]=code;}
static int projected_info(int field,union SceNetApctlInfo *out)
{assert(field==4 && session.info_users==1);unsigned value=2;memcpy(out,&value,4);return 0;}
int main(void)
{
    assert(!wifi_native_adapter_start() && running);
    int handler=add_handler(event,(void *)1);assert(handler>0);
    state=4;network_started=1;session.ready=1;session.info_callback=(unsigned)(uintptr_t)projected_info;
    static const unsigned expected_types[]={0,1,1,2,3};
    static const char *const browser_labels[]={"none","WEP","WPA-TKIP","WPA-AES"};
    for(unsigned i=0;i<5;++i) {
        struct {unsigned before,value,after;} output={0x12345678,~0u,0xabcdef01};
        profile_security=i;
        assert(!get_info(4,(void *)&output.value));
        assert(output.before==0x12345678 && output.after==0xabcdef01);
        assert(output.value==expected_types[i] && output.value<4 && !session.info_users);
        assert(browser_labels[output.value]!=NULL);
    }
    profile_security=5;unsigned invalid=0;assert(get_info(4,(void *)&invalid)<0);
    profile_security=-1;state=0;network_started=0;session.ready=0;session.info_callback=0;
    assert(!callback_owner(123) && !browser_alive());
    hooks.browser_owners[0]=123;browser_available=1;
    assert(callback_owner(123) && browser_alive());
    cancel=0;assert(!del_handler(handler) && !cancel); /* Native browser retains the link. */
    owned_net_modules=3;assert(!release_network_modules() && owned_net_modules==3 && !utility_unloads);
    browser_available=0;assert(!browser_alive());
    assert(!release_network_modules() && !owned_net_modules && utility_unloads==2);
    handler=add_handler(event,(void *)1);assert(handler>0);
    assert(!del_handler(handler) && cancel); /* Last settings owner still releases it. */
    cancel=0;handler=add_handler(event,(void *)1);assert(handler>0);
    complete=1;count=32;
    for(unsigned i=0;i<count;++i)rows[i]=(native_scan_row_t){.ssid={'a','p'},.ssid_length=2,.rssi=-47,.channel=6,.auth=NATIVE_AUTH_WPA2,.cipher=NATIVE_CIPHER_AES};
    unsigned size=0;assert(!get_list(&size,NULL) && size==160);
    struct {unsigned list[40];unsigned canary;} list={.canary=0xabcdef};size=160;
    assert(!get_list(&size,list.list) && size==160 && list.canary==0xabcdef && list.list[39]==20 && list.list[38]==0);
    struct {unsigned char field,canary;} byte={0,0xa5};assert(!get_entry(1,4,&byte.field) && byte.field==100 && byte.canary==0xa5);
    unsigned value=0;assert(!get_entry(1,5,&value) && value==4);
    rows[0].auth=NATIVE_AUTH_WPA3;assert(!get_entry(1,5,&value) && value==3);
    struct {char field[33];unsigned char canary;} name={.canary=0x5a};
    assert(!get_entry(1,1,name.field) && !strcmp(name.field,"ap") && name.canary==0x5a);
    assert(get_entry(0,1,name.field)<0 && get_entry(33,1,name.field)<0);
    assert(!scan() && pending && !complete);assert(scan()<0 && connect(1)<0);
    worker(0,NULL);assert(event_count==2 && events_seen[0]==1 && events_seen[1]==6 && !pending && !state && !complete);
    running=1;loader_result=10;start_result=10;start_status=0;reply_error=-4;event_count=0;
    assert(!scan());worker(0,NULL);
    assert(session.stage==1007 && session.detail==4 && !complete && !count && strstr(last_log,"result=fffffffc"));
    reply_error=0;
    running=1;assert(!connect(3) && selected_profile==3 && operation==WIFI_NATIVE_CONNECT_COMMAND);
    state=4;session.alive=1;
    assert(!connect(3) && connect(4)<0); /* Reuse the recovering saved profile. */
    state=0;session.alive=0;
    assert(!disconnect() && cancel && session.cancel);pending=0;
    assert(!del_handler(handler));assert(del_handler(handler)<0);
    loader_result=10;start_result=-9;
    assert(start_service()==-9 && session.stage==104 && session.detail==-9 && service==-1);
    start_result=10;start_status=-17;
    assert(start_service()==-17 && session.detail==-17 && service==-1);
    start_status=0;lookup_result=-7;
    assert(start_service()==-7 && session.stage==103 && service==-1);lookup_result=0;
    assert(!start_service() && service_started && usb.request==0);
    stop_error=-2;assert(stop_service()<0 && service_failed && service_started && service==10);
    assert(start_service()==-2);stop_error=0;
    assert(!stop_service() && service==-1 && !service_started && !service_failed);
    handler=add_handler(event,(void *)1);assert(handler>0);event_count=0;
    running=1;pending=1;cancel=0;state=4;network_started=1;session.alive=1;session.ready=1;
    simulate_reconnect=1;assert(!monitor_network());simulate_reconnect=0;
    assert(event_count==3 && events_seen[0]==10 && events_seen[1]==3 && events_seen[2]==4);
    assert(network_started && session.alive && state==4);
    network_started=0;session.alive=0;pending=0;assert(!del_handler(handler));
    assert(callback_invocations>=3 && facade_traces>0);
    assert(!wifi_native_adapter_stop());
    puts("PASS: actual native list20 bound, exact field sizes, security mapping, queued commands, events and unavailable ESP cleanup");
}
