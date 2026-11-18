#ifndef BROWSER_ADAPTER_SHIM_H
#define BROWSER_ADAPTER_SHIM_H
#include <stddef.h>
typedef int SceUID;
typedef unsigned SceSize;
#define PSP_THREAD_ATTR_USER 0
int sceKernelWaitSema(int,int,void *);
int sceKernelSignalSema(int,int);
int sceKernelGetThreadId(void);
int sceKernelDelayThread(unsigned);
int sceNetFreeThreadinfo(int);
int sceKernelCreateSema(const char *,int,int,int,void *);
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned,void *);
int sceKernelDeleteThread(int);
int sceKernelWaitThreadEnd(int,void *);
int sceKernelDeleteSema(int);
#endif
