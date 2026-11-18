/* Exercise real probe control flow against failures and pre-existing modules. */
#include "net_probe_shim.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int scenario, loads, unloads, polls, exited, syncs, native_calls;
static int (*home)(int, int, void *);
int net_probe_main(void);
int native_socket_check(int (*report)(const char *, int))
{
    assert(scenario>=10 && loads==2);
    assert(report("MOCK_NATIVE",0)==0);
    native_calls++;
    return scenario==11 ? -2 : (scenario==12 ? -1 : 0);
}
void pspDebugScreenInit(void) {}
void pspDebugScreenPrintf(const char *fmt, ...) { (void)fmt; }
SceUID sceIoOpen(const char *p, int f, int m)
{ (void)f; (void)m; assert(!strcmp(p,"ms0:/net-probe.log")); return scenario==5 ? -1 : 4; }
int sceIoWrite(SceUID f, const void *p, unsigned n)
{ assert(f==4); (void)p; return scenario==6 ? -1 : (int)n; }
int sceIoClose(SceUID f) { assert(f==4); return 0; }
int sceIoSync(const char *p, int f)
{ assert(!strcmp(p,"ms0:")); (void)f; syncs++; return scenario==7 ? -1 : 0; }
int sceIoDopen(const char *p) { assert(!strncmp(p,"flash0:/",8)); return 8; }
int sceIoDread(int f, SceIoDirent *e) { assert(f==8); (void)e; return 0; }
int sceIoDclose(int f) { assert(f==8); return 0; }
int kuKernelFindModuleByName(char *p, SceModule *m)
{ (void)p; (void)m; return -1; }
unsigned long long sceKernelGetSystemTimeWide(void) { return 123; }
unsigned sceKernelDevkitVersion(void) { return 0x06060110; }
int kuKernelGetModel(void) { return 10; }
int sctrlHENGetVersion(void) { return 1; }
int sctrlHENGetMinorVersion(void) { return 2; }
SceUID sceKernelCreateCallback(const char *n, int (*fn)(int,int,void*), void *a)
{ (void)n; (void)a; home=fn; return 7; }
int sceKernelRegisterExitCallback(SceUID c) { assert(c==7); return 0; }
int sceUtilityLoadNetModule(int m)
{
    assert(syncs && scenario!=5 && scenario!=6 && scenario!=7);
    assert(m==(loads ? PSP_NET_MODULE_INET : PSP_NET_MODULE_COMMON));
    loads++;
    if (scenario==2 || scenario==8) {
        assert(loads==1); return scenario==8 ? (int)0x80110801u : -1;
    }
    if (scenario==3 && m==PSP_NET_MODULE_INET) return -2;
    return 0;
}
int sceUtilityUnloadNetModule(int m)
{
    assert(!exited);
    if (scenario==3) assert(m==PSP_NET_MODULE_COMMON && unloads==0);
    else assert(m==(unloads ? PSP_NET_MODULE_COMMON : PSP_NET_MODULE_INET));
    unloads++;
    return scenario==4 ? -1 : 0;
}
int sceCtrlPeekBufferPositive(SceCtrlData *p, int n)
{
    (void)n;
    assert(++polls<5);
    p->Buttons = polls==1 && scenario!=0 && scenario!=9 ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
    if (scenario==9) p->Buttons=0;
    if (scenario>=10 && polls==2) p->Buttons=PSP_CTRL_SQUARE;
    return 1;
}
int sceKernelDelayThreadCB(unsigned n)
{ (void)n; if (scenario==9) home(0,0,NULL); return 0; }
void sceKernelExitGame(void) { exited++; }
int main(int argc, char **argv)
{
    assert(argc==2); scenario=atoi(argv[1]);
    assert(net_probe_main()==0 && exited==1);
    if (scenario>=10) {
        assert(native_calls==1 && loads==2);
        assert(unloads==(scenario==11 ? 0 : 2));
    } else if (scenario==0 || scenario>=5) {
        if (scenario==8) assert(loads==1 && unloads==0);
        else assert(!loads && !unloads);
    } else if (scenario==2) assert(loads==1 && !unloads);
    else if (scenario==3 || scenario==4) assert(loads==2 && unloads==1);
    else assert(loads==2 && unloads==2);
    return 0;
}
