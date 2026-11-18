#ifndef WIFI_PLUGIN_SHIM_H
#define WIFI_PLUGIN_SHIM_H
#include <stdint.h>
#include <stdarg.h>
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
void prnt(void (*)(void *,int),void *,const char *,va_list);
int sceIoOpen(const char *,int,int);
int sceIoWrite(int,const void *,unsigned);
int sceIoClose(int);
#define PSP_MODULE_INFO(...)
#define PSP_MODULE_KERNEL 0x1000
#define PSP_INIT_KEYCONFIG_VSH 0x100
#define PSP_USB_ACTIVATED 0x200
#define PSP_CTRL_LTRIGGER 0x100
#define PSP_CTRL_RTRIGGER 0x200
#define PSP_CTRL_SQUARE 0x8000
#define PSP_CTRL_CROSS 0x4000
#define PSP_CTRL_CIRCLE 0x2000
#define PSP_CTRL_TRIANGLE 0x1000
typedef int SceUID;
typedef unsigned SceSize;
typedef struct {unsigned Buttons;} SceCtrlData;
typedef struct {long long st_size;} SceIoStat;
typedef struct {char modname[28];unsigned char version[2],nsegment;unsigned text_addr,text_size,segmentaddr[4],segmentsize[4];} SceModule;
struct SceKernelLoadExecVSHParam {unsigned size,args;void *argp;const char *key;unsigned extra[5];};
SceModule *sceKernelFindModuleByName(const char *);
unsigned sctrlHENFindFunction(const char *,const char *,unsigned);
int sceUsbGetState(void);
int sceUsbGetDrvState(const char *);
unsigned sceKernelGetSystemTimeLow(void);
int sceCtrlPeekBufferPositive(SceCtrlData *,int);
int sceKernelDelayThread(unsigned);
int sceIoGetstat(const char *,SceIoStat *);
int sctrlKernelLoadExecVSHMs2(const char *,struct SceKernelLoadExecVSHParam *);
unsigned sceKernelDevkitVersion(void);
int sceKernelGetModel(void);
int sceKernelInitKeyConfig(void);
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned,void *);
int sceKernelDeleteThread(int);
int sceKernelWaitThreadEnd(int,unsigned *);
#endif
