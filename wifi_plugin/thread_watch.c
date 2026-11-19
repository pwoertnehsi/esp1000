#include "../common/esp_diagnostics.h"
/* Independent kernel observation: never wake, suspend or terminate a worker. */
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspusb.h>
#include <stdio.h>
#include <string.h>
#include "thread_watch.h"

static unsigned last_sample,last_report;
static int sampled;
static struct thread_snapshot {
    int uid,status,wait_type,wait_id,exit_status;
} previous[2];
static const char *const names[]={"ESPNativeSettings","ESPStockBrowser"};
static SceUID watch_thread=-1;
static volatile int watch_running;
static const char *const sony_modules[]={"htmlviewer_plugin_module",
    "sceHVUI_Module","sceHVNetfront_Module","SceHttp_Library",
    "vsh_module","scePaf_Module","launcher_plugin_module","rss_browser_module"};
static unsigned sony_report;
static int sony_seen;
static void sony_snapshot(unsigned now,const SceUID *ids,int count)
{
    unsigned bases[8]={0},lengths[8]={0};
    for(unsigned i=0;i<8;++i) {
        SceModule *m=sceKernelFindModuleByName(sony_modules[i]);
        if(m){bases[i]=m->text_addr;lengths[i]=m->text_size;}
    }
    if(!bases[0] && !sony_seen)return;
    if(sony_seen && now-sony_report<2000000u)return;
    SceUID fd=sceIoOpen("ms0:/browser-sony-thread.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd<0)return;
    char line[224];
    int n=snprintf(line,sizeof(line),"SNAPSHOT v=28 time=%08x html=%08x count=%d\n",now,bases[0],count);
    sceIoWrite(fd,line,(unsigned)n);
    for(int i=0;i<count;++i) {
        SceKernelThreadInfo info;memset(&info,0,sizeof(info));info.size=sizeof(info);
        if(sceKernelReferThreadStatus(ids[i],&info)<0)continue;
        unsigned entry=(unsigned)(unsigned long)info.entry,owner;
        for(owner=0;owner<8;++owner)
            if(bases[owner] && entry>=bases[owner] && entry-bases[owner]<lengths[owner])break;
        if(owner==8)continue;
        /* Only firmware-owned thread names and numeric scheduling metadata. */
        n=snprintf(line,sizeof(line),"SONY owner=%u name=%.31s uid=%08x entry=%08x status=%08x wait=%08x object=%08x exit=%08x\n",
            owner,info.name,ids[i],entry-bases[owner],info.status,info.waitType,info.waitId,info.exitStatus);
        if(n>0 && (unsigned)n<sizeof(line))sceIoWrite(fd,line,(unsigned)n);
    }
    sceIoClose(fd);sony_report=now;sony_seen=bases[0]!=0;
}

void wifi_thread_watch(unsigned now)
{
    if(!ESP1000_DIAGNOSTICS)return;
    if(sampled && now-last_sample<500000u)return;
    sampled=1;last_sample=now;
    /* Accessory USB is allowed; storage ownership is not. No file stays open. */
    if(sceUsbGetState()<0 || sceUsbGetDrvState("USBStor_Driver")==1)return;
    SceModule *bridge=sceKernelFindModuleByName("ESP WiFi XMB Bridge");
    unsigned base=bridge?bridge->text_addr:0,length=bridge?bridge->text_size:0;
    SceUID ids[128];int count=0;
    if(sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread,ids,128,&count)<0)return;
    /* A truncated enumeration cannot establish that a worker disappeared. */
    if(count<0 || count>=128)return;
    sony_snapshot(now,ids,count);
    struct thread_snapshot current[2]={{0}};
    for(int i=0;i<count;++i) {
        SceKernelThreadInfo info;memset(&info,0,sizeof(info));info.size=sizeof(info);
        if(sceKernelReferThreadStatus(ids[i],&info)<0)continue;
        unsigned entry=(unsigned)(unsigned long)info.entry;
        if(!base || entry<base || entry-base>=length)continue;
        for(unsigned slot=0;slot<2;++slot) {
            if(strncmp(info.name,names[slot],sizeof(info.name)))continue;
            current[slot]=(struct thread_snapshot){ids[i],info.status,
                info.waitType,info.waitId,info.exitStatus};
            /* Running/ready alternation is sampling noise, not a new wait. */
            if(info.status==1 || info.status==2)current[slot].status=3;
        }
    }
    int heartbeat=current[0].uid && now-last_report>=5000000u;
    if(!heartbeat && !memcmp(current,previous,sizeof(current)))return;
    char line[384];int used=0;
    for(unsigned slot=0;slot<2;++slot) {
        if(!heartbeat && !memcmp(&current[slot],&previous[slot],sizeof(current[slot])))continue;
        const struct thread_snapshot *s=&current[slot];
        used+=snprintf(line+used,sizeof(line)-(unsigned)used,
            "THREAD v=27 time=%08x slot=%u uid=%08x status=%08x wait=%08x object=%08x exit=%08x\n",
            now,slot,s->uid,s->status,s->wait_type,s->wait_id,s->exit_status);
    }
    if(used<=0 || (unsigned)used>=sizeof(line))return;
    SceUID fd=sceIoOpen("ms0:/browser-thread.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd<0)return;
    int written=sceIoWrite(fd,line,(unsigned)used);sceIoClose(fd);
    if(written!=used)return;
    memcpy(previous,current,sizeof(previous));last_report=now;
}

static int watch_worker(SceSize size,void *args)
{
    (void)size;(void)args;
    while(watch_running) {
        wifi_thread_watch(sceKernelGetSystemTimeLow());
        sceKernelDelayThread(250000);
    }
    return 0;
}
int wifi_thread_watch_start(void)
{
    if(!ESP1000_DIAGNOSTICS)return 0;
    if(watch_thread>=0)return -1;
    watch_thread=sceKernelCreateThread("ESPBrowserWatch",watch_worker,0x31,0x6000,0,NULL);
    if(watch_thread<0)return watch_thread;
    watch_running=1;int r=sceKernelStartThread(watch_thread,0,NULL);
    if(r<0){watch_running=0;sceKernelDeleteThread(watch_thread);watch_thread=-1;}
    return r;
}
int wifi_thread_watch_stop(void)
{
    watch_running=0;
    if(watch_thread<0)return 0;
    unsigned timeout=2000000;
    int r=sceKernelWaitThreadEnd(watch_thread,&timeout);
    if(r<0)return r;
    sceKernelDeleteThread(watch_thread);watch_thread=-1;return 0;
}
