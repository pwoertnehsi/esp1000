#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_resolver.h>
#include "../../common/link_protocol.h"
#include "lookup.h"
static int resolver_id=-1;
static SceUID lookup_thread=-1,stop_thread=-1;
static volatile int lookup_done,stop_done;
static int lookup_result,stop_result;
/* Native resolver may retain these until its call returns, even after timeout. */
static unsigned char buffer[1024];
static struct in_addr address;
static int lookup(SceSize size,void *args)
{
    (void)size;(void)args;
    lookup_result=sceNetResolverStartNtoA(resolver_id,LINK_TEST_HOST,&address,3,1);
    __sync_synchronize();lookup_done=1;
    sceNetFreeThreadinfo(sceKernelGetThreadId());return 0;
}
static int stop(SceSize size,void *args)
{
    (void)size;(void)args;stop_result=sceNetResolverStop(resolver_id);
    __sync_synchronize();stop_done=1;
    sceNetFreeThreadinfo(sceKernelGetThreadId());return 0;
}
static int join(SceUID *thread,volatile int *done)
{
    if(*thread<0)return 0;
    if(!*done)return -1;
    unsigned timeout=1000;
    if(sceKernelWaitThreadEnd(*thread,&timeout)<0)return -1;
    if(sceKernelDeleteThread(*thread)<0)return -1;
    *thread=-1;return 0;
}
int browser_lookup_cleanup(void)
{
    if(join(&lookup_thread,&lookup_done)<0 || join(&stop_thread,&stop_done)<0)return -1;
    if(resolver_id>=0) {
        int r=sceNetResolverDelete(resolver_id);if(r)return r;resolver_id=-1;
    }
    return 0;
}
static int lookup_attempt(int (*report)(const char *,int),int (*cancelled)(void),int *timed_out)
{
    *timed_out=0;
    if(resolver_id>=0)return -1;
    int r=sceNetResolverCreate(&resolver_id,buffer,sizeof(buffer));report("DNS_CREATE",r);if(r)return -1;
    lookup_done=stop_done=0;lookup_result=stop_result=-1;
    lookup_thread=sceKernelCreateThread("BrowserDnsLookup",lookup,0x28,0x10000,PSP_THREAD_ATTR_USER,NULL);
    if(lookup_thread<0){browser_lookup_cleanup();return -1;}
    r=sceKernelStartThread(lookup_thread,0,NULL);report("DNS_WORKER_START",r);
    if(r<0){lookup_done=1;if(!sceKernelDeleteThread(lookup_thread))lookup_thread=-1;browser_lookup_cleanup();return -1;}
    unsigned start=sceKernelGetSystemTimeLow(),last=~0u;int aborted=0;
    while(!lookup_done) {
        unsigned elapsed=sceKernelGetSystemTimeLow()-start;
        if(cancelled()){aborted=1;break;}
        if(elapsed>=8000000u){*timed_out=1;aborted=1;break;}
        if(elapsed/1000000u!=last){last=elapsed/1000000u;report("DNS_WAIT_SECONDS",last);}
        sceKernelDelayThread(20000);
    }
    if(aborted) {
        report("DNS_CANCEL_BEGIN",-1);
        stop_thread=sceKernelCreateThread("BrowserDnsStop",stop,0x27,0x4000,PSP_THREAD_ATTR_USER,NULL);
        if(stop_thread>=0) {
            r=sceKernelStartThread(stop_thread,0,NULL);report("DNS_CANCEL_START",r);
            if(r<0){stop_done=1;if(!sceKernelDeleteThread(stop_thread))stop_thread=-1;}
        }
        unsigned deadline=sceKernelGetSystemTimeLow()+2000000u;
        while((!lookup_done || (stop_thread>=0 && !stop_done)) && (int)(deadline-sceKernelGetSystemTimeLow())>0)
            sceKernelDelayThread(20000);
        if(stop_done)report("DNS_CANCEL_RESULT",stop_result);
    }
    if(lookup_done)report("NATIVE_DNS_LOOKUP",lookup_result);
    r=browser_lookup_cleanup();report("DNS_CLEANUP",r);
    if(r){report("DNS_RESOURCES_RETAINED",-1);return -1;}
    return aborted?-1:lookup_result;
}

int browser_lookup_run(int (*report)(const char *,int),int (*cancelled)(void))
{
    /* Some native resolver calls outlive their requested socket timeout. Retry
     * a lost query only after Stop, both joins and Delete have completed. Never
     * reuse live resolver state, or retry user cancellation / USB link loss. */
    for(unsigned attempt=1;attempt<=2;++attempt) {
        if(cancelled())return -1;
        report("DNS_ATTEMPT",attempt);
        int timed_out=0;
        int result=lookup_attempt(report,cancelled,&timed_out);
        if(!result || !timed_out || resolver_id>=0 || cancelled())return result;
    }
    return -1;
}
