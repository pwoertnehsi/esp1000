/* ESP1000's game-mode transport for the GPL-3.0-or-later aemu fork.
 * Calls run in game threads, never inside a ModuleMgr start/stop callback. */
#include <pspkernel.h>
#include <psploadcore.h>
#include <psputility.h>
#include <psputility_netparam.h>
#include <psputility_usbmodules.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet.h>
#include <kubridge.h>
#include <string.h>
#include <netinet/in.h>
#include "native_sony_profile.h"
#include "adhoc_portmap.h"
#include "wifi_plugin/native_control.h"
#include "virtual_net/control.h"
#include "virtual_net/app/native_config.h"
#include "native_route.h"
#include "lab_gateway.h"
static wifi_native_usb_control_t usb={.magic=WIFI_NATIVE_USB_MAGIC,.abi=WIFI_NATIVE_USB_ABI};
static volatile vnet_start_result_t transport;
static int driver=-1,driver_started,helper=-1,helper_started,configured,state,profile_index;
static unsigned request,map_nonce;
extern int espAdhocPowerAcquire(void);
extern int espAdhocPowerRelease(void);

static void log_result(const char *name,int value)
{
    char line[96];unsigned n=0;
    const char *prefix="ESP_ADHOC ";while(*prefix)line[n++]=*prefix++;
    for(unsigned i=0;name[i] && n<80;++i)line[n++]=name[i];
    line[n++]='=';static const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<8;++i)line[n++]=hex[((unsigned)value>>(28-4*i))&15];
    line[n++]='\n';
    int fd=sceIoOpen("ms0:/esp-adhoc.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd>=0){sceIoWrite(fd,line,n);sceIoClose(fd);}
}
static int report(const char *name,int value){log_result(name,value);return 0;}
static int get_parameter(int index,int field,void *out)
{return sceUtilityGetNetParam(index,field,(netData *)out);}
static int stop_helper(void)
{
    if(helper_started){int status=-1;int r=sceKernelStopModule(helper,0,NULL,&status,NULL);if(r<0 || status<0)return -1;helper_started=0;}
    if(helper>=0){if(sceKernelUnloadModule(helper)<0)return -1;helper=-1;}return 0;
}
static int required_ready(void)
{
    unsigned required=VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY;
    return driver_started && (transport.flags&required)==required;
}
int espApctlDisconnect(void);
static int install_route(void)
{
    unsigned char message[VNET_ROUTE_SIZE],reply[1024];vnet_default_route(message);
    int fd=sceNetInetSocket(17,SOCK_RAW,0),result=-1;if(fd<0)return -1;
    if(sceNetInetSend(fd,message,sizeof(message),MSG_DONTWAIT)==sizeof(message)) {
        unsigned began=sceKernelGetSystemTimeLow();
        while(sceKernelGetSystemTimeLow()-began<3000000u) {
            int n=sceNetInetRecv(fd,reply,sizeof(reply),MSG_DONTWAIT);
            if(n>=76 && reply[2]==3 && reply[3]==1 && link_get_u32(reply+20)==VNET_ROUTE_SEQ) {
                unsigned error=link_get_u32(reply+24);result=(error==0 || error==17)?0:-1;break;
            }
            sceKernelDelayThread(1000);
        }
    }
    if(sceNetInetClose(fd))result=-1;
    return result;
}
int espApctlInit(int stack,int priority)
{(void)stack;(void)priority;configured=1;return 0;}
int espApctlConnect(int index)
{
    if(!configured || driver>=0 || helper>=0)return (int)0x80410a0bu;
    native_wifi_profile_t profile;
    if(native_sony_profile_read(index,get_parameter,&profile))return -1;
    profile_index=index;state=2;
    /* Acquire before either USB owner starts; retain across Disconnect/retry. */
    int power=espAdhocPowerAcquire();log_result("POWER_ACQUIRE",power);
    if(power<0){native_profile_clear(&profile,sizeof(profile));state=0;return power;}
    /* A loaded USB ACC module is acceptable; driver start checks ownership. */
    int r=sceUtilityLoadUsbModule(PSP_USB_MODULE_ACC);log_result("USB_ACC_LOAD",r);
    SceModule owner;
    if(kuKernelFindModuleByAddress((void *)espApctlConnect,&owner)<0)goto fail;
    helper=kuKernelLoadModule("ms0:/seplugins/esp-wifi/wifi_native_usb.prx",0,NULL);
    if(helper<0)goto fail;
    wifi_native_usb_args_t args={WIFI_NATIVE_USB_MAGIC,(unsigned)&usb,owner.modid};int status=-1;
    r=sceKernelStartModule(helper,sizeof(args),&args,&status,NULL);
    if(r<0 || status<0)goto fail;
    helper_started=1;usb.command=WIFI_NATIVE_CONNECT_COMMAND;usb.profile=profile;
    if(!++request)++request;
    __sync_synchronize();usb.request=request;native_profile_clear(&profile,sizeof(profile));
    unsigned began=sceKernelGetSystemTimeLow();
    while(usb.completed!=request && sceKernelGetSystemTimeLow()-began<22000000u)sceKernelDelayThread(10000);
    if(usb.completed!=request || usb.result || stop_helper())goto fail;
    driver=kuKernelLoadModule("ms0:/PSP/GAME/psp-browser/psp_vnet.prx",0,NULL);
    if(driver<0)goto fail;
    vnet_start_args_t start={VNET_START_MAGIC,(unsigned)&transport};status=-1;
    r=sceKernelStartModule(driver,sizeof(start),&start,&status,NULL);
    if(r<0 || status<0)goto fail;
    driver_started=1;if(transport.result)goto fail;
    began=sceKernelGetSystemTimeLow();
    while(!required_ready() && sceKernelGetSystemTimeLow()-began<15000000u)sceKernelDelayThread(10000);
    if(!required_ready())goto fail;
    if(vnet_configure_lab(report) || install_route())goto fail;
    state=4;log_result("CONNECTED_PROFILE",index);return 0;
fail:
    native_profile_clear(&profile,sizeof(profile));
    stop_helper();espApctlDisconnect();state=0;log_result("CONNECT_FAILED_PHASE",usb.phase);return -1;
}
int espApctlGetState(int *out)
{if(!out)return -1;*out=state==4 && !required_ready()?2:state;return 0;}
static void ip_text(char *out,const unsigned char *ip)
{
    for(unsigned i=0;i<4;++i) {
        unsigned value=ip[i];if(value>=100)*out++='0'+value/100;
        if(value>=10)*out++='0'+value/10%10;
        *out++='0'+value%10;if(i!=3)*out++='.';
    }
    *out=0;
}
int espApctlGetInfo(int field,union SceNetApctlInfo *out)
{
    if(!out || state!=4 || !required_ready())return -1;
    vnet_start_result_t current;if(!vnet_status_snapshot(&current,&transport))return -1;
    switch(field) {
    case PSP_NET_APCTL_INFO_IP:ip_text(out->ip,vnet_lab_psp_ip);return 0;
    case PSP_NET_APCTL_INFO_SUBNETMASK:{const unsigned char mask[4]={255,255,255,0};ip_text(out->subNetMask,mask);return 0;}
    case PSP_NET_APCTL_INFO_GATEWAY:ip_text(out->gateway,vnet_lab_gateway_ip);return 0;
    case PSP_NET_APCTL_INFO_PRIMDNS:ip_text(out->primaryDns,current.wifi_info);return 0;
    case PSP_NET_APCTL_INFO_SECDNS:ip_text(out->secondaryDns,current.wifi_info+4);return 0;
    case PSP_NET_APCTL_INFO_STRENGTH:out->strength=current.wifi_info[46];return 0;
    case PSP_NET_APCTL_INFO_SSID:memcpy(out->ssid,current.wifi_info+8,32);return 0;
    default:{netData value;int parameter=field==PSP_NET_APCTL_INFO_PROFILE_NAME?0:-1;
        if(parameter<0 || sceUtilityGetNetParam(profile_index,parameter,&value))return -1;
        memcpy(out,&value,64);return 0;}
    }
}
int espLocalMac(void *out)
{if(!out)return -1;memcpy(out,vnet_lab_psp_mac,6);return 0;}
int espWlanSwitch(void){return 1;}
static int mapping(unsigned op,unsigned protocol,unsigned port)
{
    if(!required_ready())return -1;
    int socket=sceNetInetSocket(AF_INET,SOCK_DGRAM,0);if(socket<0)return -1;
    struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_len=sizeof(address);address.sin_family=AF_INET;
    address.sin_port=__builtin_bswap16(27313);memcpy(&address.sin_addr,vnet_lab_gateway_ip,4);
    uint8_t packet[16],reply[16];unsigned nonce=__sync_add_and_fetch(&map_nonce,1);if(!nonce)nonce=__sync_add_and_fetch(&map_nonce,1);adhoc_map_request(packet,op,protocol,port,nonce);
    int result=-1;
    for(unsigned retry=0;retry<3 && result<0;++retry) {
        sceNetInetSendto(socket,packet,16,0,(struct sockaddr *)&address,sizeof(address));
        unsigned began=sceKernelGetSystemTimeLow();
        while(sceKernelGetSystemTimeLow()-began<300000u) {
            struct sockaddr_in from;socklen_t from_size=sizeof(from);
            int n=sceNetInetRecvfrom(socket,reply,sizeof(reply),MSG_DONTWAIT,(struct sockaddr *)&from,&from_size);
            int32_t status;
            if(n==16 && from.sin_port==address.sin_port && from.sin_addr.s_addr==address.sin_addr.s_addr && adhoc_map_reply(packet,reply,n,&status)){result=status;break;}
            sceKernelDelayThread(1000);
        }
    }
    sceNetInetClose(socket);log_result("PORTMAP",result);return result;
}
void espPortOpen(const char *protocol,unsigned short port)
{mapping(ADHOC_PORTMAP_ADD,!strcmp(protocol,"TCP")?6:17,port);}
void espPortClose(const char *protocol,unsigned short port)
{mapping(ADHOC_PORTMAP_REMOVE,!strcmp(protocol,"TCP")?6:17,port);}
void espMapsStart(void){}
void espMapsStop(void){mapping(ADHOC_PORTMAP_CLEAR,0,0);}
int espApctlDisconnect(void)
{
    state=0;
    if(driver_started) {
        int status=-1;int r=sceKernelStopModule(driver,0,NULL,&status,NULL);
        if(r<0 || status<0){log_result("STOP_UNSAFE",status);return -1;}driver_started=0;
    }
    if(driver>=0){if(sceKernelUnloadModule(driver)<0)return -1;driver=-1;}
    return stop_helper();
}
int espApctlTerm(void)
{int r=espApctlDisconnect();if(!r)r=espAdhocPowerRelease();if(!r)configured=0;return r;}
