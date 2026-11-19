/* A read-only filesystem for the private browser modules and resource. This keeps
 * Sony's load-to-block API and allocation bookkeeping intact. Its module-load
 * ioctls apply only to our own descriptors, never the global ms0 driver. */
#include <pspkernel.h>
#include <pspiofilemgr_kernel.h>
#include <string.h>
#include <stdint.h>
#include "asset_device.h"
static const char *const files[]={"htmlviewer_ui.prx","netfront.prx","libslim.prx","libwww.prx","mm_flash.prx"};
static volatile int active,stopping;
static int installed;
static volatile browser_status_t *report;
static int record(unsigned command,int result)
{if(report){report->asset_command=command;report->asset_result=result;++report->asset_events;}return result;}
static int path(const char *name,char out[96])
{
    const char prefix[]="/vsh/module/";
    if(!name)return -1;
    if(!strcmp(name,"/vsh/resource/htmlviewer.res")) {
        strcpy(out,"ms0:/PSP/GAME/psp-browser/b0");strcat(out,name);return 0;
    }
    if(strncmp(name,prefix,sizeof(prefix)-1))return -1;
    for(unsigned i=0;i<5;++i)if(!strcmp(name+sizeof(prefix)-1,files[i])) {
        strcpy(out,"ms0:/PSP/GAME/psp-browser/b1");strcat(out,name);return 0;
    }
    return -1;
}
static int init(PspIoDrvArg *arg){(void)arg;return 0;}
static int finish(PspIoDrvArg *arg){(void)arg;return active?-1:0;}
static int open_file(PspIoDrvFileArg *arg,char *name,int flags,SceMode mode)
{
    (void)mode;char target[96];
    if(arg->fs_num || path(name,target)<0 || (flags&~0x04000001) || !(flags&1))return record(1,-1);
    int irq=sceKernelCpuSuspendIntr();
    if(stopping){sceKernelCpuResumeIntr(irq);return -1;}
    ++active;sceKernelCpuResumeIntr(irq);
    int fd=sceIoOpen(target,PSP_O_RDONLY,0);
    if(fd<0){irq=sceKernelCpuSuspendIntr();--active;sceKernelCpuResumeIntr(irq);return record(1,fd);}
    arg->arg=(void *)(uintptr_t)(fd+1);return record(1,0);
}
static int descriptor(PspIoDrvFileArg *arg){return arg->arg?(int)(uintptr_t)arg->arg-1:-1;}
static int close_file(PspIoDrvFileArg *arg)
{
    int fd=descriptor(arg);if(fd<0)return -1;
    int r=sceIoClose(fd);if(r<0)return r;
    arg->arg=NULL;int irq=sceKernelCpuSuspendIntr();--active;sceKernelCpuResumeIntr(irq);return 0;
}
static int read_file(PspIoDrvFileArg *arg,char *data,int length)
{int fd=descriptor(arg);return fd<0 || length<0?-1:sceIoRead(fd,data,length);}
static SceOff seek_file(PspIoDrvFileArg *arg,SceOff offset,int whence)
{int fd=descriptor(arg);return fd<0?-1:sceIoLseek(fd,offset,whence);}
static int ioctl_file(PspIoDrvFileArg *arg,unsigned command,void *in,int inlen,void *out,int outlen)
{
    if(descriptor(arg)<0 || in || inlen || out || outlen)return -1;
    /* ModuleMgr uses 208006 for ordinary modules, 208007 for load-to-block.
     * This filesystem explicitly supports both read-only load operations.
     * Reject the optional encrypted/compressed-device probes (208081/82), so
     * the loader follows its ordinary file/decryption path. */
    return record(command,command==0x00208006 || command==0x00208007?0:-1);
}
static int stat_file(PspIoDrvFileArg *arg,const char *name,SceIoStat *stat)
{char target[96];return arg->fs_num || path(name,target)<0?-1:sceIoGetstat(target,stat);}
static PspIoDrvFuncs functions={.IoInit=init,.IoExit=finish,.IoOpen=open_file,.IoClose=close_file,
    .IoRead=read_file,.IoLseek=seek_file,.IoIoctl=ioctl_file,.IoGetstat=stat_file};
static PspIoDrv driver={"brows",0x10,0x800,"Browser assets",&functions};
int browser_asset_device_start(volatile browser_status_t *status)
{
    if(installed)return -1;
    report=status;stopping=0;int r=sceIoAddDrv(&driver);if(!r)installed=1;return r;
}
int browser_asset_device_stop(void)
{
    if(!installed)return 0;
    int irq=sceKernelCpuSuspendIntr();stopping=1;int busy=active;sceKernelCpuResumeIntr(irq);
    if(busy)return -1;
    int r=sceIoDelDrv("brows");if(!r)installed=0;return r;
}
