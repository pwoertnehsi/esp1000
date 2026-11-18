#ifndef PSP_LOADER_TEST_SHIM_H
#define PSP_LOADER_TEST_SHIM_H
#define PSP_MODULE_INFO(...)
#define PSP_MAIN_THREAD_ATTR(...)
#define PSP_THREAD_ATTR_USER 0
#define PSP_USB_MODULE_ACC 2
#define PSP_CTRL_CIRCLE 0x2000
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
typedef int SceUID;
typedef struct { unsigned Buttons; } SceCtrlData;
void pspDebugScreenInit(void);
void pspDebugScreenPrintf(const char *, ...);
SceUID sceIoOpen(const char *, int, int);
int sceIoWrite(SceUID, const void *, unsigned);
int sceIoClose(SceUID);
SceUID sceKernelCreateCallback(const char *, int (*)(int, int, void *), void *);
int sceKernelRegisterExitCallback(SceUID);
int sceUtilityLoadUsbModule(int);
SceUID kuKernelLoadModule(const char *, int, void *);
int sceKernelStartModule(SceUID, unsigned, void *, int *, void *);
int sceKernelStopModule(SceUID, unsigned, void *, int *, void *);
int sceKernelUnloadModule(SceUID);
int sceCtrlPeekBufferPositive(SceCtrlData *, int);
int sceKernelDelayThreadCB(unsigned);
void sceKernelExitGame(void);
#endif
