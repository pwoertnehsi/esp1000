#ifndef NATIVE_USB_SHIM
#define NATIVE_USB_SHIM
#include <stdint.h>
typedef int SceUID;typedef unsigned SceSize;
#define PSP_MODULE_INFO(...)
#define PSP_MODULE_KERNEL 0x1000
typedef struct {int modid;unsigned text_addr,nsegment,segmentaddr[4],segmentsize[4];} SceModule;
SceModule *sceKernelFindModuleByUID(int);
unsigned sceKernelDevkitVersion(void);unsigned sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned);
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned,void *);int sceKernelDeleteThread(int);int sceKernelWaitThreadEnd(int,unsigned *);
#endif
