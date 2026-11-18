#include "browser_lookup_shim.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../browser_local/app/lookup.c"
static unsigned now;
static int mode,deleted,cancelled,retained,created,attempts;
static struct {int (*fn)(SceSize,void *);int started,finished;} jobs[4];
unsigned sceKernelGetSystemTimeLow(void){return now;}
int sceKernelDelayThread(unsigned n)
{
    now+=n;
    for(int i=0;i<created;++i) {
        if(!jobs[i].started || jobs[i].finished)continue;
        if(jobs[i].fn==stop || (((mode==0 || mode==7 || (mode==8 && attempts==2)) && now>=40000) ||
            (cancelled && mode!=3 && mode!=6))) {
            jobs[i].fn(0,NULL);jobs[i].finished=1;
        }
    }
    return 0;
}
int sceKernelGetThreadId(void){return 100;}
int sceNetFreeThreadinfo(int id){(void)id;return 0;}
int sceKernelCreateThread(const char *name,int (*fn)(SceSize,void *),int p,int s,int a,void *arg)
{(void)name;(void)p;(void)s;(void)a;(void)arg;if(mode==5)return -1;int i=created++;assert(i<4);jobs[i].fn=fn;return i+10;}
int sceKernelStartThread(int id,unsigned n,void *arg)
{(void)n;(void)arg;if((mode==4 && id==10) || (mode==6 && id==11))return -1;jobs[id-10].started=1;return 0;}
int sceKernelWaitThreadEnd(int id,void *p){assert(p);return jobs[id-10].finished?0:-1;}
int sceKernelDeleteThread(int id){assert(!jobs[id-10].started || jobs[id-10].finished);return 0;}
int sceNetResolverCreate(int *rid,void *buf,unsigned size){assert(buf && size==1024);cancelled=0;*rid=55;return 0;}
int sceNetResolverStartNtoA(int rid,const char *host,struct in_addr *addr,unsigned timeout,int retry)
{assert(rid==55 && host && addr && timeout==3 && retry==1);addr->s_addr=1;return cancelled || mode==7?-9:0;}
int sceNetResolverStop(int rid){assert(rid==55);cancelled=1;return 0;}
int sceNetResolverDelete(int rid){assert(rid==55 && (lookup_thread<0 && stop_thread<0));++deleted;return 0;}
static int report(const char *phase,int result){(void)result;if(!strcmp(phase,"DNS_RESOURCES_RETAINED"))++retained;if(!strcmp(phase,"DNS_ATTEMPT"))++attempts;return 0;}
static int aborting(void){return mode==2 && now>=40000;}
int main(int argc,char **argv)
{
    assert(argc==2);mode=atoi(argv[1]);int result=browser_lookup_run(report,aborting);
    assert(now<=20040000);
    if(mode==0 || mode==8){assert(!result && deleted==(mode==8?2:1) && !cancelled);if(mode==8)assert(attempts==2);}
    else assert(result<0);
    if(mode==1)assert(cancelled && deleted==2 && attempts==2 && now>=16000000);
    if(mode==2)assert(cancelled && deleted==1 && now<8000000);
    if(mode==3 || mode==6) {
        assert(retained==1 && !deleted && browser_lookup_cleanup()<0 && resolver_id==55);
        jobs[0].fn(0,NULL);jobs[0].finished=1;
        assert(!browser_lookup_cleanup() && deleted==1);
    } else assert(!browser_lookup_cleanup() && resolver_id<0 && deleted==((mode==1 || mode==8)?2:1));
    puts("PASS: bounded native DNS, cancellation and retained live resources");
}
