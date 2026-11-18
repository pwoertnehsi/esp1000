#ifndef NATIVE_SESSION_SHIM
#define NATIVE_SESSION_SHIM
#include <stdint.h>
typedef int SceUID;typedef unsigned SceSize;
#define PSP_MODULE_INFO(...)
#define PSP_MAIN_THREAD_ATTR(...)
#define PSP_THREAD_ATTR_USER 0
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
int sceIoOpen(const char *,int,int);int sceIoWrite(int,const void *,unsigned);int sceIoClose(int);
typedef struct {int pool,maximum,free;} SceNetMallocStat;
typedef struct {unsigned nsegment,segmentsize[4],segmentaddr[4];} SceModule;
union SceNetApctlInfo {unsigned data[32];};
typedef void (*sceNetApctlHandler)(int,int,int,int,void *);
int sceNetGetMallocStat(SceNetMallocStat *);int sceNetInit(int,int,int,int,int);int sceNetTerm(void);
int sceNetInetInit(void);int sceNetInetTerm(void);int sceNetResolverInit(void);int sceNetResolverTerm(void);
int sceNetFreeThreadinfo(int);int sceKernelGetThreadId(void);
unsigned sceKernelGetSystemTimeLow(void);int sceKernelDelayThread(unsigned);
int kuKernelLoadModule(const char *,int,void *);int kuKernelFindModuleByName(char *,SceModule *);
int sceKernelStartModule(int,unsigned,void *,int *,void *);int sceKernelStopModule(int,unsigned,void *,int *,void *);int sceKernelUnloadModule(int);
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned,void *);int sceKernelDeleteThread(int);int sceKernelWaitThreadEnd(int,unsigned *);
#endif
