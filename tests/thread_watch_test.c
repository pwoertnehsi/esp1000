#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "wifi_plugin_shim.h"
#define SCE_KERNEL_TMID_Thread 1
typedef struct {
    unsigned size;char name[32];void *entry;int status,waitType,waitId,exitStatus;
} SceKernelThreadInfo;
int sceKernelGetThreadmanIdList(int,SceUID *,int,int *);
int sceKernelReferThreadStatus(SceUID,SceKernelThreadInfo *);
#include "../wifi_plugin/thread_watch.c"
static SceModule bridge={.text_addr=0x10000,.text_size=0x1000};
static int storage,present=1,full,foreign_entry,thread_status=4,wait_id=42,opens;
static char output[8192];static unsigned bytes;
SceModule *sceKernelFindModuleByName(const char *name)
{assert(!strcmp(name,"ESP WiFi XMB Bridge"));return &bridge;}
int sceUsbGetState(void){return PSP_USB_ACTIVATED;}
int sceUsbGetDrvState(const char *name)
{assert(!strcmp(name,"USBStor_Driver"));return storage?1:2;}
int sceKernelGetThreadmanIdList(int type,SceUID *ids,int capacity,int *count)
{assert(type==1 && capacity==128);*count=full?128:present;ids[0]=99;return 0;}
int sceKernelReferThreadStatus(SceUID uid,SceKernelThreadInfo *info)
{assert(uid==99 && info->size==sizeof(*info));strcpy(info->name,"ESPNativeSettings");
 info->entry=(void *)(uintptr_t)(foreign_entry?0x20000:0x10040);
 info->status=thread_status;info->waitType=3;info->waitId=wait_id;return 0;}
int sceIoOpen(const char *name,int flags,int mode)
{(void)flags;(void)mode;assert(!storage && !strcmp(name,"ms0:/browser-thread.log"));++opens;return 8;}
int sceIoWrite(int fd,const void *data,unsigned size)
{assert(fd==8 && bytes+size<sizeof(output));memcpy(output+bytes,data,size);bytes+=size;output[bytes]=0;return (int)size;}
int sceIoClose(int fd){assert(fd==8);return 0;}
int main(void)
{
 wifi_thread_watch(0);assert(opens==1 && strstr(output,"uid=00000063") && strstr(output,"object=0000002a"));
 wifi_thread_watch(100000);assert(opens==1);
 wifi_thread_watch(500000);assert(opens==1);
 storage=1;thread_status=8;wifi_thread_watch(1000000);assert(opens==1);
 storage=0;wifi_thread_watch(1500000);assert(opens==2 && strstr(output,"status=00000008"));
 full=1;present=0;wifi_thread_watch(2000000);assert(opens==2);
 full=0;wifi_thread_watch(2500000);assert(opens==3 && strstr(output,"uid=00000000"));
 present=1;foreign_entry=1;wifi_thread_watch(3000000);assert(opens==3);
 foreign_entry=0;thread_status=1;wifi_thread_watch(3500000);assert(opens==4);
 thread_status=2;wifi_thread_watch(4000000);assert(opens==4);
 wifi_thread_watch(8500000);assert(opens==5); /* Independent heartbeat. */
 puts("PASS: kernel thread watch ownership, storage, truncation, transitions, heartbeat");
}
