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

void wifi_thread_watch(unsigned now)
{
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
