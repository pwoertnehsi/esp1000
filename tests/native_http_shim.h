#ifndef NATIVE_HTTP_SHIM_H
#define NATIVE_HTTP_SHIM_H
#include "native_socket_shim.h"
#ifndef SO_NONBLOCK
#define SO_NONBLOCK 0x1009
#endif
unsigned sceKernelGetSystemTimeLow(void);
int sceNetInetConnect(int,const struct sockaddr *,socklen_t);
int sceNetInetSetsockopt(int,int,int,const void *,socklen_t);
int sceNetInetGetsockopt(int,int,int,void *,socklen_t *);
size_t sceNetInetSend(int,const void *,size_t,int);
size_t sceNetInetRecv(int,void *,size_t,int);
#endif
