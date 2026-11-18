#define _GNU_SOURCE
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "native_usb_shim.h"
#include "../wifi_plugin/native_usb/main.c"
static unsigned now;
static int mode,alive=1,starts,stops,deleted;
static SceModule bridge={.modid=1,.text_addr=0x09000000,.nsegment=2,.segmentaddr={0x09000000,0x09001000},.segmentsize={0x1000,0x3000}};
SceModule *sceKernelFindModuleByUID(int id){return id==1 && alive?&bridge:NULL;}
unsigned sceKernelDevkitVersion(void){return 0x06060010;}
unsigned sceKernelGetSystemTimeLow(void){return now;}
int sceKernelDelayThread(unsigned n)
{now+=n;if(control->completed)running=0;if(mode==4)alive=0;if(mode==6)running=0;return 0;}
int sceKernelCreateThread(const char *n,int (*fn)(SceSize,void *),int a,int b,int c,void *d){(void)n;(void)fn;(void)a;(void)b;(void)c;(void)d;return 3;}
int sceKernelStartThread(int a,unsigned b,void *c){(void)a;(void)b;(void)c;return 0;}
int sceKernelDeleteThread(int a){assert(a==3);++deleted;return 0;}
int sceKernelWaitThreadEnd(int a,unsigned *b){assert(a==3 && b);return 0;}
int vnet_usb_native_mode(int n){assert(n==1);return 0;}
unsigned vnet_usb_start_step(void){return mode==5?3:0;}
int vnet_usb_start(void){++starts;return mode==5?-1:0;}
int vnet_usb_stop(void){++stops;return mode==2?-1:0;}
int vnet_usb_poll(void *p,unsigned n){assert(p && n==PACKET_MAX);return 0;}
int vnet_usb_ready(void){return mode!=1 && mode!=4 && mode!=6;}
int vnet_usb_scan_begin(unsigned request,unsigned limit){assert(request==1 && limit==32);return 0;}
int vnet_usb_scan_snapshot(native_scan_session_t *p){memset(p,0,sizeof(*p));if(mode==7){p->error=NATIVE_SCAN_BAD_WIRE;return -1;}p->complete=1;p->count=1;p->rows[0].ssid_length=2;memcpy(p->rows[0].ssid,"ap",2);return 1;}
int vnet_usb_profile_begin(unsigned request,const native_wifi_profile_t *p){assert(request==1 && native_profile_valid(p) && !memcmp(p->ssid,"ap",2));return 0;}
int vnet_usb_profile_status(void){return 1;}
unsigned vnet_usb_uplink(unsigned char ip[4]){memset(ip,0,4);return 1;}
int vnet_usb_wifi_info(unsigned char radio[48]){memset(radio,0,48);memcpy(radio+8,"ap",2);return 1;}
int main(int argc,char **argv)
{
    assert(argc==2);mode=atoi(argv[1]);void *memory=mmap((void *)0x09000000,0x10000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);assert(memory==(void *)0x09000000);
    wifi_native_usb_control_t *shared=(void *)0x09001000;
    *shared=(wifi_native_usb_control_t){.magic=WIFI_NATIVE_USB_MAGIC,.abi=WIFI_NATIVE_USB_ABI,.request=1,.command=mode==3?WIFI_NATIVE_CONNECT_COMMAND:WIFI_NATIVE_SCAN_COMMAND};
    if(mode==3)shared->profile=(native_wifi_profile_t){.ssid_length=2,.key_length=8,.security=4,.ssid={'a','p'},.key={'1','2','3','4','5','6','7','8'}};
    wifi_native_usb_args_t args={WIFI_NATIVE_USB_MAGIC,0x09001000,1};
    assert(module_start(sizeof(args)-1,&args)<0 && !starts);
    args.address=0x88000000;assert(module_start(sizeof(args),&args)<0 && !starts);args.address=0x09001000;
    assert(!module_start(sizeof(args),&args));worker(0,NULL);
    assert(starts==1 && stops>=1);
    if(mode==0 || mode==3)assert(shared->completed==1 && !shared->result);
    else if(mode!=4)assert(shared->completed==1 && shared->result<0);
    if(mode==4)assert(!shared->completed);
    if(mode==7)assert(shared->result==-(int)NATIVE_SCAN_BAD_WIRE && shared->phase==7 && shared->detail==NATIVE_SCAN_BAD_WIRE);
    for(unsigned i=0;i<sizeof(shared->profile);++i)assert(!((unsigned char *)&shared->profile)[i]);
    if(mode==2)assert(module_stop(0,NULL)==-2 && !deleted && opened && unsafe);
    else assert(!module_stop(0,NULL) && deleted==1 && !opened);
    munmap(memory,0x10000);puts("PASS: actual native USB owner validation, scan/profile, timeout, cancellation, owner loss and unsafe buffer retention");
}
