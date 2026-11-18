#ifndef ADHOC_POWER_SHIM_H
#define ADHOC_POWER_SHIM_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#define PSP_INIT_KEYCONFIG_GAME 0x200
#define PSP_USB_ACTIVATED 0x200
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
typedef unsigned SceSize;
typedef struct { unsigned text_size; unsigned char version[2]; uintptr_t text_addr; } SceModule2;
static SceModule2 test_module;
static unsigned text[0x97c8/4], now, boot_waits, power_on, power_off;
static int resolve_error;
static int model=10, context=0x200, usb_state, foreign, on_error, off_error;
static unsigned sctrlHENFindFunction(char *module,char *library,unsigned nid)
{
    (void)module;(void)library;assert(nid==0x3ac3d2a4);
    return resolve_error ? 0 : (text[0x9020/4]&0x03ffffff)<<2;
}
static int sceKernelGetModel(void){return model;}
static int sceKernelInitKeyConfig(void){return context;}
static int sceUsbGetState(void){return usb_state;}
static int sceUsbGetDrvState(const char *name){(void)name;return foreign?1:2;}
static void *sceKernelFindModuleByName(const char *name){(void)name;return &test_module;}
static int sceKernelCpuSuspendIntr(void){return 1;}
static void sceKernelCpuResumeIntr(int intr){(void)intr;}
static void sceKernelDcacheWritebackAll(void){}
static void sceKernelIcacheInvalidateAll(void){}
static int pspSdkSetK1(int value){(void)value;return 0;}
static unsigned sceKernelGetSystemTimeLow(void){return now;}
static void sceKernelDelayThread(unsigned usec){now+=usec;if(usec==2000000)++boot_waits;}
static int sceIoOpen(const char *path,int flags,int mode){(void)path;(void)flags;(void)mode;return -1;}
static int sceIoWrite(int fd,const void *buffer,int size){(void)fd;(void)buffer;return size;}
static int sceIoClose(int fd){(void)fd;return 0;}
#endif
