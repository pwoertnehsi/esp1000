#ifndef BROWSER_ASSET_SHIM_H
#define BROWSER_ASSET_SHIM_H
#include <stdint.h>
typedef int SceMode;
typedef long long SceOff;
typedef struct {int value;} SceIoStat;
typedef struct {void *arg;} PspIoDrvArg;
typedef struct {unsigned fs_num;void *arg;} PspIoDrvFileArg;
typedef struct {
 int (*IoInit)(PspIoDrvArg *);int (*IoExit)(PspIoDrvArg *);
 int (*IoOpen)(PspIoDrvFileArg *,char *,int,SceMode);int (*IoClose)(PspIoDrvFileArg *);
 int (*IoRead)(PspIoDrvFileArg *,char *,int);SceOff (*IoLseek)(PspIoDrvFileArg *,SceOff,int);
 int (*IoIoctl)(PspIoDrvFileArg *,unsigned,void *,int,void *,int);
 int (*IoGetstat)(PspIoDrvFileArg *,const char *,SceIoStat *);
} PspIoDrvFuncs;
typedef struct {const char *name;unsigned dev_type,unk2;const char *name2;PspIoDrvFuncs *funcs;} PspIoDrv;
#define PSP_O_RDONLY 1
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int);
int sceIoOpen(const char *,int,SceMode);
int sceIoClose(int);
int sceIoRead(int,void *,unsigned);
SceOff sceIoLseek(int,SceOff,int);
int sceIoGetstat(const char *,SceIoStat *);
int sceIoAddDrv(PspIoDrv *);
int sceIoDelDrv(const char *);
#endif
