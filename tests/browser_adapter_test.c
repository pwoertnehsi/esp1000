#include "browser_adapter_shim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../browser_local/app/adapter.c"
static vnet_start_result_t status;
static int calls,delays,mode;
int sceKernelWaitSema(int a,int b,void *c){(void)a;(void)b;(void)c;return 0;}
int sceKernelSignalSema(int a,int b){(void)a;(void)b;return 0;}
int sceKernelGetThreadId(void){return worker;}
int sceKernelDelayThread(unsigned us){(void)us;if(++delays==1)status.flags=0;else running=0;return 0;}
int sceNetFreeThreadinfo(int id){(void)id;return 0;}
int sceKernelCreateSema(const char *n,int a,int b,int c,void *d){(void)n;(void)a;(void)b;(void)c;(void)d;return 1;}
int sceKernelCreateThread(const char *n,int (*fn)(SceSize,void *),int a,int b,int c,void *d){(void)n;(void)fn;(void)a;(void)b;(void)c;(void)d;return mode==1?-1:2;}
int sceKernelStartThread(int a,unsigned b,void *c){(void)a;(void)b;(void)c;return mode==2?-1:0;}
int sceKernelDeleteThread(int id){(void)id;return 0;}
int sceKernelWaitThreadEnd(int id,void *p){(void)id;(void)p;return 0;}
int sceKernelDeleteSema(int id){(void)id;return 0;}
static void callback(int old,int now,int event,int error,void *arg)
{assert(old==4 && now==0 && event==PSP_NET_APCTL_EVENT_ERROR && error==-1 && arg==&calls);++calls;}
int main(void)
{
    for(mode=1;mode<=2;++mode){assert(adapter_start(&status)<0);assert(lock<0 && worker<0);}
    mode=0;status.flags=VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY;
    status.wifi_info[0]=192;status.wifi_info[1]=168;status.wifi_info[2]=1;status.wifi_info[3]=1;
    memcpy(status.wifi_info+8,"Pixel6a",7);status.wifi_info[46]=87;status.wifi_info[47]=0x36;
    assert(!adapter_start(&status));int state;assert(!adapter_get_state(&state) && state==4);
    assert(adapter_connect(1)<0 && !adapter_get_state(&state) && state==4);
    struct {unsigned char before[8],value[128],after[8];} field;
    const struct {int code;unsigned size;} cases[]={
        {PSP_NET_APCTL_INFO_SSID,32},{PSP_NET_APCTL_INFO_SSID_LENGTH,4},{PSP_NET_APCTL_INFO_STRENGTH,1},
        {PSP_NET_APCTL_INFO_CHANNEL,1},{PSP_NET_APCTL_INFO_IP,16},{PSP_NET_APCTL_INFO_BSSID,6},
        {PSP_NET_APCTL_INFO_PROXY_URL,128},{PSP_NET_APCTL_INFO_PROXY_PORT,2},{PSP_NET_APCTL_INFO_SECURITY_TYPE,4}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);++i){
        memset(&field,0xa5,sizeof(field));assert(!adapter_get_info(cases[i].code,(void *)field.value));
        for(unsigned j=0;j<8;++j)assert(field.before[j]==0xa5 && field.after[j]==0xa5);
        for(unsigned j=cases[i].size;j<128;++j)assert(field.value[j]==0xa5);
    }
    status.revision=1;assert(adapter_get_info(PSP_NET_APCTL_INFO_IP,(void *)field.value)<0);
    status.revision=2;assert(!adapter_disconnect() && !adapter_get_state(&state) && state==0);
    assert(adapter_get_info(PSP_NET_APCTL_INFO_IP,(void *)field.value)<0);assert(!adapter_connect(0));
    int ids[16];for(unsigned i=0;i<16;++i){ids[i]=adapter_add_handler(callback,&calls);assert(ids[i]>=0);}
    assert(adapter_add_handler(callback,&calls)<0);
    for(unsigned i=1;i<16;++i)assert(!adapter_del_handler(ids[i]));
    assert(adapter_del_handler(ids[1])<0);
    events(0,NULL);assert(calls==1 && !adapter_get_state(&state) && !state);
    assert(!adapter_pause());adapter_dispose();assert(lock<0 && !link);
    puts("PASS: adapter field-size canaries, real link gating, snapshots, profile refusal, handler limits/loss, partial startup cleanup");
}
