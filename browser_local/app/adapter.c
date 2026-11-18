/* Test-session APCTL projection backed by the real configured USB/IP link.
 * No profile keys are read. Connect(0) reuses this preconfigured adapter only;
 * other profile indices are rejected instead of silently using another AP. */
#include <pspkernel.h>
#include <pspnet.h>
#include <string.h>
#include "adapter.h"
static volatile vnet_start_result_t *link;
static SceUID lock=-1,worker=-1;
static volatile int running,requested;
static unsigned serial;
static struct {sceNetApctlHandler fn;void *arg;int id,active,busy;} handlers[16];
static void enter(void){sceKernelWaitSema(lock,1,NULL);}
static void leave(void){sceKernelSignalSema(lock,1);}
static int connected(void)
{
    unsigned flags=link?link->flags:0;
    return running && requested && (flags&(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY))
        ==(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY);
}
int adapter_get_state(int *state)
{if(!state)return -1;*state=connected()?PSP_NET_APCTL_STATE_GOT_IP:PSP_NET_APCTL_STATE_DISCONNECTED;return 0;}
static void ip_string(char *out,const unsigned char *ip)
{
    for(unsigned i=0;i<4;++i) {
        unsigned n=ip[i];if(n>=100)*out++='0'+n/100;
        if(n>=10)*out++='0'+n/10%10;
        *out++='0'+n%10;if(i!=3)*out++='.';
    }
    *out=0;
}
int adapter_get_info(int code,union SceNetApctlInfo *info)
{
    if(!info || !connected())return -1;
    union SceNetApctlInfo value;memset(&value,0,sizeof(value));unsigned size=0;
    vnet_start_result_t sample;
    if(!vnet_status_snapshot(&sample,link) || (sample.flags&(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY))
       !=(VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY))return -1;
    unsigned char radio[48];memcpy(radio,sample.wifi_info,48);
    switch(code) {
    case PSP_NET_APCTL_INFO_PROFILE_NAME:strcpy(value.name,"ESP adapter (preconfigured)");size=64;break;
    case PSP_NET_APCTL_INFO_BSSID:memcpy(value.bssid,radio+40,6);size=6;break;
    case PSP_NET_APCTL_INFO_SSID:memcpy(value.ssid,radio+8,32);size=32;break;
    case PSP_NET_APCTL_INFO_SSID_LENGTH:while(value.ssidLength<32 && radio[8+value.ssidLength])++value.ssidLength;size=4;break;
    case PSP_NET_APCTL_INFO_SECURITY_TYPE: {
        unsigned auth=radio[47]>>4;
        if(auth>4)return -1;
        value.securityType=auth==0?0:auth==1?1:2;size=4;break;
    }
    case PSP_NET_APCTL_INFO_STRENGTH:value.strength=radio[46];size=1;break;
    case PSP_NET_APCTL_INFO_CHANNEL:value.channel=radio[47]&15;size=1;break;
    case PSP_NET_APCTL_INFO_POWER_SAVE:size=1;break;
    case PSP_NET_APCTL_INFO_IP:strcpy(value.ip,"192.168.77.2");size=16;break;
    case PSP_NET_APCTL_INFO_SUBNETMASK:strcpy(value.subNetMask,"255.255.255.0");size=16;break;
    case PSP_NET_APCTL_INFO_GATEWAY:strcpy(value.gateway,"192.168.77.1");size=16;break;
    case PSP_NET_APCTL_INFO_PRIMDNS:ip_string(value.primaryDns,radio);size=16;break;
    case PSP_NET_APCTL_INFO_SECDNS:ip_string(value.secondaryDns,radio+4);size=16;break;
    case PSP_NET_APCTL_INFO_PROXY_URL:size=128;break;
    case PSP_NET_APCTL_INFO_PROXY_PORT:size=2;break;
    case PSP_NET_APCTL_INFO_USE_PROXY:case PSP_NET_APCTL_INFO_8021_EAP_TYPE:
    case PSP_NET_APCTL_INFO_START_BROWSER:case PSP_NET_APCTL_INFO_WIFISP:size=4;break;
    default:return -1;
    }
    /* Sony callers sometimes allocate only the requested field, not a union. */
    memcpy(info,&value,size);return 0;
}
int adapter_connect(int profile)
{
    if(profile!=0 || !running)return -1;
    requested=1;
    if(!connected()){requested=0;return -1;}
    return 0;
}
int adapter_disconnect(void){requested=0;return 0;}
int adapter_add_handler(sceNetApctlHandler fn,void *arg)
{
    if(!fn || !running)return -1;
    enter();int id=-1;
    for(unsigned i=0;i<16;++i)if(!handlers[i].active && !handlers[i].busy) {
        if(++serial>0xffff)serial=1;
        id=0x100000+(serial<<4)+i;
        handlers[i].fn=fn;handlers[i].arg=arg;handlers[i].id=id;handlers[i].active=1;break;
    }
    leave();return id;
}
int adapter_del_handler(int id)
{
    if(lock<0)return -1;
    unsigned slot=(unsigned)id&15;
    enter();
    if(handlers[slot].id!=id || !handlers[slot].active){leave();return -1;}
    handlers[slot].active=0;
    while(handlers[slot].busy && sceKernelGetThreadId()!=worker) {
        leave();sceKernelDelayThread(1000);enter();
    }
    leave();return 0;
}
static int events(SceSize size,void *args)
{
    (void)size;(void)args;int old=connected()?4:0;
    while(running) {
        int state=connected()?4:0;
        if(state!=old) {
            for(unsigned i=0;i<16 && running;++i) {
                enter();sceNetApctlHandler fn=handlers[i].active?handlers[i].fn:NULL;
                void *arg=handlers[i].arg;if(fn)handlers[i].busy=1;leave();
                if(fn) {
                    fn(old,state,state?PSP_NET_APCTL_EVENT_GET_IP:requested?PSP_NET_APCTL_EVENT_ERROR:PSP_NET_APCTL_EVENT_DISCONNECT_REQUEST,
                       !state && requested?-1:0,arg);
                    enter();handlers[i].busy=0;leave();
                }
            }
            old=state;
        }
        sceKernelDelayThread(20000);
    }
    sceNetFreeThreadinfo(sceKernelGetThreadId());return 0;
}
int adapter_start(volatile vnet_start_result_t *transport)
{
    link=transport;requested=1;running=1;
    lock=sceKernelCreateSema("BrowserAdapterLock",0,1,1,NULL);
    if(lock<0){running=0;return -1;}
    worker=sceKernelCreateThread("BrowserAdapterEvents",events,0x30,0x8000,PSP_THREAD_ATTR_USER,NULL);
    if(worker<0){adapter_dispose();return -1;}
    int r=sceKernelStartThread(worker,0,NULL);
    if(r<0){sceKernelDeleteThread(worker);worker=-1;adapter_dispose();}return r;
}
int adapter_pause(void)
{
    running=0;requested=0;
    if(worker>=0){int r=sceKernelWaitThreadEnd(worker,NULL);if(r<0)return -1;sceKernelDeleteThread(worker);worker=-1;}
    return 0;
}
void adapter_dispose(void)
{
    running=0;requested=0;
    if(lock>=0){sceKernelDeleteSema(lock);lock=-1;}
    memset(handlers,0,sizeof(handlers));link=NULL;
}
