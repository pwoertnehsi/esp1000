#ifndef BROWSER_HELPER_SHIM_H
#define BROWSER_HELPER_SHIM_H
#include <stdint.h>
#define PSP_MODULE_INFO(...)
#define PSP_MODULE_KERNEL 0x1000
#define SCE_KERNEL_ERROR_UNKNOWN_MODULE (-1)
typedef int SceUID;
typedef unsigned SceSize;
typedef struct {char modname[28];unsigned char version[2],nsegment;unsigned text_addr,text_size,segmentaddr[4],segmentsize[4];int modid;} SceModule;
typedef int (*STMOD_HANDLER)(SceModule *);
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int);
void sceKernelDcacheWritebackRange(const void *,unsigned);
void sceKernelIcacheInvalidateRange(const void *,unsigned);
unsigned sctrlHENFindFunction(const char *,const char *,unsigned);
unsigned sceKernelDevkitVersion(void);
int sceKernelGetModel(void);
SceModule *sceKernelFindModuleByUID(int);
SceModule *sceKernelFindModuleByName(const char *);
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER);
#endif
