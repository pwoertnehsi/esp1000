#ifndef BROWSER_STATE_SHIM_H
#define BROWSER_STATE_SHIM_H
#include "browser_helper_shim.h"
typedef struct {unsigned size;char name[32];unsigned status,waitType;int waitId;} SceKernelThreadInfo;
int kuKernelFindModuleByName(const char *,SceModule *);
int sceKernelReferThreadStatus(int,SceKernelThreadInfo *);
#endif
