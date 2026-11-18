#ifndef BROWSER_LOOKUP_SHIM_H
#define BROWSER_LOOKUP_SHIM_H
#include "browser_adapter_shim.h"
#include <netinet/in.h>
unsigned sceKernelGetSystemTimeLow(void);
int sceNetResolverCreate(int *,void *,unsigned);
int sceNetResolverStartNtoA(int,const char *,struct in_addr *,unsigned,int);
int sceNetResolverStop(int);
int sceNetResolverDelete(int);
#endif
