#include "browser_asset_shim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../browser_local/driver/asset_device.c"
static int exists,opens,closes,fail_open,fail_close;
int sceKernelCpuSuspendIntr(void){return 0;}
void sceKernelCpuResumeIntr(int n){(void)n;}
int sceIoOpen(const char *path,int flags,SceMode mode)
{assert((!strcmp(path,"ms0:/PSP/GAME/psp-browser/b1/vsh/module/netfront.prx") || !strcmp(path,"ms0:/PSP/GAME/psp-browser/b0/vsh/resource/htmlviewer.res")) && flags==1 && mode==0);++opens;return fail_open?-7:0;}
int sceIoClose(int fd){assert(fd==0);if(fail_close)return -8;++closes;return 0;}
int sceIoRead(int fd,void *data,unsigned n){assert(fd==0);memset(data,0x33,n);return n;}
SceOff sceIoLseek(int fd,SceOff offset,int whence){assert(fd==0 && whence==0);return offset;}
int sceIoGetstat(const char *path,SceIoStat *stat){assert((!strcmp(path,"ms0:/PSP/GAME/psp-browser/b1/vsh/module/netfront.prx") || !strcmp(path,"ms0:/PSP/GAME/psp-browser/b0/vsh/resource/htmlviewer.res")));stat->value=123;return 0;}
int sceIoAddDrv(PspIoDrv *drv){assert(!strcmp(drv->name,"brows"));if(exists)return -1;exists=1;return 0;}
int sceIoDelDrv(const char *name){assert(!strcmp(name,"brows") && !active);exists=0;return 0;}
int main(void)
{
    exists=1;assert(browser_asset_device_start(NULL)<0 && !installed);assert(!browser_asset_device_stop() && exists);
    exists=0;assert(!browser_asset_device_start(NULL));PspIoDrvFileArg arg={0};
    assert(open_file(&arg,"/vsh/module/../../kd/loadcore.prx",1,0)<0);
    assert(open_file(&arg,"/vsh/module/netfront.prx",2,0)<0);
    assert(open_file(&arg,"/vsh/module/netfront.prx",0x201,0)<0 && !opens);
    arg.fs_num=1;assert(open_file(&arg,"/vsh/module/netfront.prx",1,0)<0);arg.fs_num=0;
    fail_open=1;assert(open_file(&arg,"/vsh/module/netfront.prx",1,0)==-7 && !active);fail_open=0;
    assert(!open_file(&arg,"/vsh/module/netfront.prx",0x04000001,0) && active==1);
    char data[8];assert(read_file(&arg,data,8)==8 && data[7]==0x33);
    assert(read_file(&arg,data,-1)<0 && seek_file(&arg,128,0)==128);
    assert(!ioctl_file(&arg,0x208006,NULL,0,NULL,0));assert(!ioctl_file(&arg,0x208007,NULL,0,NULL,0));
    assert(ioctl_file(&arg,0x208082,NULL,0,NULL,0)<0 && ioctl_file(&arg,0x208007,data,8,NULL,0)<0);
    SceIoStat stat;assert(!stat_file(&arg,"/vsh/module/netfront.prx",&stat) && stat.value==123);
    assert(browser_asset_device_stop()<0 && installed && exists);
    PspIoDrvFileArg other={0};assert(open_file(&other,"/vsh/module/netfront.prx",1,0)<0);
    fail_close=1;assert(close_file(&arg)==-8 && active);fail_close=0;
    assert(!close_file(&arg) && !active && closes==1);
    assert(close_file(&arg)<0 && read_file(&arg,data,8)<0 && ioctl_file(&arg,0x208007,NULL,0,NULL,0)<0);
    assert(!browser_asset_device_stop() && !exists && !installed);
    assert(!browser_asset_device_start(NULL));
    PspIoDrvFileArg resource={0};
    assert(!open_file(&resource,"/vsh/resource/htmlviewer.res",1,0));
    assert(open_file(&arg,"/vsh/resource/../module/netfront.prx",1,0)<0);
    assert(open_file(&arg,"/vsh/resource/other.res",1,0)<0);
    assert(!stat_file(&resource,"/vsh/resource/htmlviewer.res",&stat));
    assert(!close_file(&resource));assert(!browser_asset_device_stop());
    puts("PASS: browser asset allowlist, read-only flags, fd0, read/seek, scoped load ioctls, foreign driver and live-file unload refusal");
}
