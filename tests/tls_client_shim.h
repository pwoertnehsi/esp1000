#ifndef TLS_CLIENT_SHIM_H
#define TLS_CLIENT_SHIM_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef int SceUID;
typedef struct {int modid;} SceModule;
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
#define AF_INET 2
#define MSG_DONTWAIT 128
struct sockaddr_in {unsigned char sin_len,sin_family;unsigned short sin_port;uint32_t sin_addr;char rest[8];};
int sceKernelWaitSema(int,int,void *);int sceKernelSignalSema(int,int);
unsigned sceKernelGetSystemTimeLow(void);int sceKernelDelayThread(unsigned);
int sceKernelCreateSema(const char *,int,int,int,void *);
int sceIoOpen(const char *,int,int);int sceIoWrite(int,const void *,unsigned);int sceIoClose(int);
int sceNetInetGetErrno(void);int sceNetInetSend(int,const void *,unsigned,int);int sceNetInetRecv(int,void *,unsigned,int);int sceNetInetShutdown(int,int);
int kuKernelFindModuleByName(char *,SceModule *);
#endif
