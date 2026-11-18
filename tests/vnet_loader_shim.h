#ifndef VNET_LOADER_SHIM_H
#define VNET_LOADER_SHIM_H
#include "psp_loader_shim.h"
#include "native_socket_shim.h"
#define PSP_CTRL_SQUARE 0x8000
#define PSP_CTRL_CROSS 0x4000
#define PSP_NET_MODULE_COMMON 1
#define PSP_NET_MODULE_INET 3
int sceIoSync(const char *,int);
int sceUtilityLoadNetModule(int);
int sceUtilityUnloadNetModule(int);
#endif
#ifndef VNET_THREAD_SHIM_H
#define VNET_THREAD_SHIM_H
typedef unsigned SceSize;
typedef struct { unsigned size, status; int waitType, waitId; } SceKernelThreadInfo;
SceUID sceKernelCreateThread(const char *, int (*)(SceSize,void *),int,unsigned,unsigned,void *);
int sceKernelStartThread(SceUID,unsigned,void *);
int sceKernelWaitThreadEnd(SceUID,void *);
int sceKernelDeleteThread(SceUID);
int sceKernelReferThreadStatus(SceUID,SceKernelThreadInfo *);
SceUID sceKernelGetThreadId(void);
unsigned sceKernelGetSystemTimeLow(void);
int sceNetFreeThreadinfo(int);
int sceNetThreadAbort(int);
#endif

int sceUtilityLoadUsbModule(int);
int sceUtilityUnloadUsbModule(int);

#define PSP_CTRL_TRIANGLE 0x1000

#define PSP_CTRL_START 0x0008
