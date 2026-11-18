#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_session_shim.h"
#include "../wifi_plugin/native_session/main.c"
int sceIoOpen(const char *p,int f,int m){(void)p;(void)f;(void)m;assert(!driver_started);return 9;}
int sceIoWrite(int f,const void *p,unsigned n){assert(f==9 && p && n<80);return n;}
int sceIoClose(int f){assert(f==9);return 0;}
static unsigned now;
static int loss_started,recovery_ticks,mode,net_inits,net_terms,inet_terms,resolver_terms,stopped,unloaded,configured,restored,paused;
int sceNetGetMallocStat(SceNetMallocStat *s){s->pool=mode==1 || mode==7?0:128*1024;return 0;}
int sceNetInit(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;++net_inits;return mode==7?-1:0;}
int sceNetTerm(void){++net_terms;return 0;}
int sceNetInetInit(void){return mode==1?0:(int)0x80410201u;}
int sceNetInetTerm(void){++inet_terms;return 0;}
int sceNetResolverInit(void){return mode==1?0:(int)0x80000020u;}
int sceNetResolverTerm(void){++resolver_terms;return 0;}
int sceNetFreeThreadinfo(int a){(void)a;return 0;}int sceKernelGetThreadId(void){return 2;}
unsigned sceKernelGetSystemTimeLow(void){return now;}
int sceKernelDelayThread(unsigned n)
{
    now+=n;
    unsigned required=VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY;
    if(mode==5){if(control->ready)transport.flags=0;}
    else if(mode==11) {
        if(control->ready){control->info_users=1;control->cancel=1;}
        else if(control->info_users){assert(!paused && !stopped);control->info_users=0;}
    }
    else if(mode==6 || mode==8 || mode==9) {
        if(control->ready && !loss_started){transport.flags=0;loss_started=1;}
        else if(control->recovering) {
            assert(control->alive && !control->ready && !stopped && !unloaded);
            ++recovery_ticks;
            if(mode==8)control->cancel=1;
            else if(recovery_ticks%5==0)transport.flags=required;
        } else if(control->ready && loss_started) {
            if(mode==9 && control->recoveries==1){transport.flags=0;}
            else control->cancel=1;
        }
    } else if(control->ready)control->cancel=1;
    return 0;
}
int kuKernelLoadModule(const char *p,int f,void *a){(void)p;(void)f;(void)a;return 10;}
int kuKernelFindModuleByName(char *n,SceModule *m){(void)n;(void)m;return -1;}
int sceKernelStartModule(int id,unsigned n,void *p,int *status,void *a)
{
    (void)a;assert(id==10 && n==sizeof(vnet_start_args_t));vnet_start_args_t *r=p;
    assert(r->magic==VNET_START_MAGIC);*status=0;transport.result=0;
    transport.flags=VNET_USB_READY|VNET_UPLINK_READY|VNET_WIFI_INFO_READY;return 0;
}
int sceKernelStopModule(int id,unsigned n,void *p,int *status,void *a){(void)n;(void)p;(void)a;assert(id==10);++stopped;*status=mode==4?-2:0;return 0;}
int sceKernelUnloadModule(int id){assert(id==10);++unloaded;return 0;}
int sceKernelCreateThread(const char *n,int (*fn)(SceSize,void *),int a,int b,int c,void *d){(void)n;(void)fn;(void)a;(void)b;(void)c;(void)d;return 2;}
int sceKernelStartThread(int id,unsigned n,void *a){(void)id;(void)n;(void)a;return 0;}
int sceKernelDeleteThread(int id){(void)id;return 0;}int sceKernelWaitThreadEnd(int id,unsigned *p){(void)id;(void)p;return 0;}
int vnet_run_local_http(int (*r)(const char *,int)){return mode==2?-1:r("probe",0);}
int vnet_run_unbound_local_http(int (*r)(const char *,int)){return r("probe",0);}
int vnet_install_default_route(int (*r)(const char *,int)){return r("route",0);}
int browser_dns_configure(const unsigned char a[4],const unsigned char b[4]){(void)a;(void)b;++configured;return 0;}
int browser_dns_restore(void){++restored;return 0;}
int browser_lookup_run(int (*r)(const char *,int),int (*c)(void)){(void)c;return mode==3?-1:r("dns",0);}
int browser_lookup_cleanup(void){return 0;}
int adapter_start(volatile vnet_start_result_t *s){assert(s==&transport);return 0;}
int adapter_pause(void){++paused;return 0;}void adapter_dispose(void){}
int adapter_get_info(int n,union SceNetApctlInfo *p){(void)n;(void)p;return 0;}
int main(int argc,char **argv)
{
    assert(argc==2);mode=atoi(argv[1]);wifi_native_session_control_t shared={.magic=WIFI_NATIVE_SESSION_MAGIC,.abi=WIFI_NATIVE_SESSION_ABI};control=&shared;
    worker(0,NULL);assert(!shared.ready && !shared.info_callback);
    if(mode==0 || mode==1 || mode==6 || mode==8 || mode==9 || mode==11)assert(!shared.result && paused==1);
    else assert(shared.result<0);
    if(mode==5)assert(now>=PSP_LINK_RECOVERY_US && shared.recoveries==1);
    if(mode==6)assert(shared.recoveries==1 && recovery_ticks==5);
    if(mode==8)assert(shared.recoveries==1 && now<PSP_LINK_RECOVERY_US);
    if(mode==9)assert(shared.recoveries==2 && recovery_ticks==10);
    assert(!shared.alive && !shared.recovering);
    if(mode==1)assert(net_inits==1 && net_terms==1 && inet_terms==1 && resolver_terms==1);
    else assert(!net_terms && !inet_terms && !resolver_terms);
    if(mode==4)assert(unsafe && !unloaded && module_stop(0,NULL)==-2);
    else if(mode!=7)assert(stopped==1 && unloaded==1 && !unsafe);
    if(mode==2 || mode==7)assert(!configured);
    if(mode==5) {
        /* Retry after an expired outage without recreating VSH / the process. */
        mode=0;shared.cancel=0;shared.result=-1;
        worker(0,NULL);
        assert(!shared.result && !shared.ready && !shared.alive);
        assert(stopped==2 && unloaded==2 && paused==2 && !unsafe);
    }
    puts("PASS: native session borrowed/owned NET, real readiness, probe/DNS failure, link loss, cleanup and retained unsafe driver");
}
