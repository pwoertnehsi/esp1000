#ifndef NATIVE_ADAPTER_SHIM
#define NATIVE_ADAPTER_SHIM
#include <stdint.h>
typedef int SceUID;typedef unsigned SceSize;
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
int sceUsbGetState(void);int sceUsbGetDrvState(const char *);
int sceIoOpen(const char *,int,int);int sceIoWrite(int,const void *,unsigned);int sceIoClose(int);
#define PSP_THREAD_ATTR_USER 0
#define PSP_USB_MODULE_ACC 3
#define PSP_NET_MODULE_COMMON 1
#define PSP_NET_MODULE_INET 3
int sceUtilityLoadNetModule(int);int sceUtilityUnloadNetModule(int);
#define PSP_NET_APCTL_INFO_SECURITY_TYPE 4
union SceNetApctlInfo {unsigned securityType;unsigned char data[128];};
typedef struct {int modid;unsigned text_addr,text_size,gp_value;} SceModule;
typedef union {unsigned asUint;char asString[128];} netData;
int sceKernelWaitSema(int,int,void *);int sceKernelSignalSema(int,int);
int sceKernelGetThreadId(void);unsigned sceKernelGetSystemTimeLow(void);
int sceKernelDelayThread(unsigned);int sceKernelCreateSema(const char *,int,int,int,void *);
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned,void *);int sceKernelDeleteThread(int);
int sceKernelWaitThreadEnd(int,unsigned *);int sceKernelDeleteSema(int);
int sceKernelStartModule(int,unsigned,void *,int *,void *);
int sceKernelStopModule(int,unsigned,void *,int *,void *);
int sceKernelUnloadModule(int);int sceKernelGetModuleIdByAddress(const void *);
int sceUtilityLoadUsbModule(int);int sceUtilityUnloadUsbModule(int);
int sceUtilityGetNetParam(int,int,netData *);
int kuKernelFindModuleByAddress(void *,SceModule *);int kuKernelFindModuleByName(char *,SceModule *);
int kuKernelLoadModule(const char *,int,void *);
#endif
