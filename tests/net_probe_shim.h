#ifndef NET_PROBE_SHIM_H
#define NET_PROBE_SHIM_H
#include "psp_loader_shim.h"
#define PSP_CTRL_SQUARE 0x8000
#define PSP_CTRL_CROSS 0x4000
#define PSP_NET_MODULE_COMMON 1
#define PSP_NET_MODULE_INET 3
typedef struct {
    int modid; unsigned attribute, version[2], text_addr, text_size;
    void *ent_top, *stub_top;
    unsigned ent_size, stub_size, nsegment;
} SceModule;
typedef struct { char d_name[256]; struct { unsigned long long st_size; } d_stat; } SceIoDirent;
int sceIoSync(const char *, int);
int sceIoDopen(const char *);
int sceIoDread(int, SceIoDirent *);
int sceIoDclose(int);
int kuKernelFindModuleByName(char *, SceModule *);
unsigned long long sceKernelGetSystemTimeWide(void);
unsigned sceKernelDevkitVersion(void);
int kuKernelGetModel(void);
int sctrlHENGetVersion(void);
int sctrlHENGetMinorVersion(void);
int sceUtilityLoadNetModule(int);
int sceUtilityUnloadNetModule(int);
#endif
