/* Run the actual user launcher with mocked firmware calls. The exit path
 * must stop/join the kernel module before asking firmware to exit the game. */
#include "psp_loader_shim.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int scenario, stopped, unloaded, exited, sleeps;
static int (*home_callback)(int, int, void *);
int psp_loader_main(int, char **);
void pspDebugScreenInit(void) {}
void pspDebugScreenPrintf(const char *fmt, ...) { (void)fmt; }
SceUID sceIoOpen(const char *p, int f, int m) { (void)p; (void)f; (void)m; return -1; }
int sceIoWrite(SceUID f, const void *p, unsigned n) { (void)f; (void)p; return n; }
int sceIoClose(SceUID f) { (void)f; return 0; }
SceUID sceKernelCreateCallback(const char *n, int (*fn)(int,int,void*), void *a)
{ (void)n; (void)a; home_callback = fn; return 7; }
int sceKernelRegisterExitCallback(SceUID c) { assert(c == 7); return 0; }
int sceUtilityLoadUsbModule(int m) { assert(m == 2); return 0; }
SceUID kuKernelLoadModule(const char *p, int f, void *o)
{ (void)f; (void)o; assert(strstr(p, "/psp_link.prx")); return scenario == 2 ? -1 : 42; }
int sceKernelStartModule(SceUID m, unsigned n, void *a, int *s, void *o)
{ (void)n; (void)a; (void)o; assert(m == 42); *s = 0; return 42; }
int sceKernelStopModule(SceUID m, unsigned n, void *a, int *s, void *o)
{ (void)n; (void)a; (void)o; assert(!exited && m == 42); stopped++; *s=0; return 42; }
int sceKernelUnloadModule(SceUID m) { assert(stopped && !exited && m==42); unloaded++; return 0; }
int sceCtrlPeekBufferPositive(SceCtrlData *p, int n)
{ (void)n; p->Buttons = scenario == 0 ? PSP_CTRL_CIRCLE : 0; return 1; }
int sceKernelDelayThreadCB(unsigned n)
{ (void)n; assert(++sleeps < 3); assert(home_callback); home_callback(0,0,NULL); return 0; }
void sceKernelExitGame(void)
{ assert(scenario == 2 || (stopped == 1 && unloaded == 1)); exited++; }
int main(int argc, char **argv)
{
    assert(argc==2); scenario=atoi(argv[1]);
    char *args[] = {"ms0:/PSP/GAME/psp-link/EBOOT.PBP", NULL};
    assert(psp_loader_main(1,args)==0);
    assert(exited==1);
    if (scenario != 0) assert(sleeps==1);
    if (scenario==2) assert(!stopped && !unloaded);
    return 0;
}
