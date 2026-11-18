/* Compile the real launcher, including its two-stage failure cleanup. */
#include "vnet_loader_shim.h"
#include "../virtual_net/control.h"
#include <assert.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
static volatile vnet_start_result_t *gate_result;
static int scenario, polls, fatal, exited, started, stops, driver_unloads, lab_calls;
static int common, inet_module, net, inet, net_terms, inet_terms, sleeps;
static int (*home)(int,int,void *);
struct fake_thread {int (*entry)(unsigned,void *); int finished, abort;};
static struct fake_thread threads[32];
static int next_thread=20,active_thread,blocked_id,abort_uid,aborts,queries;
static unsigned fake_time;
static void execute_thread(int id)
{
    int prior=active_thread;active_thread=id;
    threads[id].entry(0,NULL);threads[id].finished=1;active_thread=prior;
}
SceUID sceKernelCreateThread(const char *name,int (*entry)(unsigned,void *),int p,unsigned stack,unsigned attr,void *opt)
{
    (void)p;(void)stack;(void)attr;(void)opt;assert(next_thread<32);
    int id=next_thread++;threads[id].entry=entry;threads[id].abort=strstr(name,"Abort")!=NULL;
    if(threads[id].abort)abort_uid=id;
    return id;
}
int sceKernelStartThread(int id,unsigned size,void *args)
{
    (void)size;(void)args;
    if(scenario>=19 && !threads[id].abort && started && lab_calls==(getenv("HTTP_CANCEL_TEST")?1:0) && !blocked_id){blocked_id=id;return 0;}
    execute_thread(id);return 0;
}
int sceKernelWaitThreadEnd(int id,void *timeout)
{ (void)timeout;assert(threads[id].finished);return 0; }
int sceKernelDeleteThread(int id)
{ assert(threads[id].finished);if(id==abort_uid)abort_uid=0;return 0; }
int sceKernelReferThreadStatus(int id,SceKernelThreadInfo *info)
{ assert(id==blocked_id);queries++;info->status=4;info->waitType=3;info->waitId=0x99;return 0; }
int sceKernelGetThreadId(void) { assert(active_thread);return active_thread; }
unsigned sceKernelGetSystemTimeLow(void)
{ fake_time+=scenario==21 || scenario==22 ? 6000000 : 20000;return fake_time; }
int sceNetFreeThreadinfo(int id) { assert(active_thread==id);return 0; }
int sceNetThreadAbort(int id)
{
    assert(active_thread==abort_uid && id==blocked_id);aborts++;
    if(scenario!=22){execute_thread(id);blocked_id=0;}
    return 0;
}
static jmp_buf stopped;
int vnet_loader_main(int,char **);
int vnet_run_lab_client(int (*report)(const char *,int))
{
    assert(scenario>=16 && net && inet && started);
    if (getenv("USB_GATE_TEST")) assert(polls>=4);
    lab_calls++;
    int r=report("MOCK_LAB",0);
    if(scenario>=19){assert(aborts==1 && r<0);return -1;}
    return scenario==17 ? -1 : 0;
}
void pspDebugScreenInit(void) {}
void pspDebugScreenPrintf(const char *format,...) { (void)format; }
SceUID sceIoOpen(const char *p,int f,int m)
{ (void)f;(void)m;assert(!strcmp(p,"ms0:/psp-vnet.log"));return 8; }
int sceIoWrite(int f,const void *p,unsigned n) { assert(f==8);(void)p;return n; }
int sceIoClose(int f) { assert(f==8);return 0; }
int sceIoSync(const char *p,int f) { assert(!strcmp(p,"ms0:"));(void)f;return 0; }
SceUID sceKernelCreateCallback(const char *n,int (*fn)(int,int,void *),void *a)
{ (void)n;(void)a;home=fn;return 7; }
int sceKernelRegisterExitCallback(int c) { assert(c==7);return 0; }
int sceUtilityLoadNetModule(int m)
{
    if (m==1) { if (scenario==3 || scenario==13) return -1; common=1; }
    else { assert(m==3 && common); if (scenario==4) return -1; inet_module=1; }
    return 0;
}
int sceUtilityUnloadNetModule(int m)
{
    assert(!net && !inet && !started);
    if (m==3) { assert(inet_module);inet_module=0; }
    else { assert(m==1 && common && !inet_module);common=0; }
    return 0;
}
int sceNetInit(int p,int a,int b,int c,int d)
{ (void)p;(void)a;(void)b;(void)c;(void)d;assert(common && inet_module);if(scenario==5)return -1;net=1;return 0; }
int sceNetInetInit(void) { assert(net);if(scenario==6)return -1;inet=1;return 0; }
int sceNetInetTerm(void)
{
    assert(inet && (!started || ((scenario==10 || scenario==11 || scenario==15 || scenario==18) && stops==1)));
    inet_terms++;
    if (scenario==12) { fatal=1;return -1; }
    inet=0;return 0;
}
int sceNetTerm(void)
{
    assert(net && !inet && !blocked_id && !abort_uid);net_terms++;
    if (scenario==11) { fatal=1;return -1; }
    net=0;return 0;
}
SceUID kuKernelLoadModule(const char *p,int f,void *o)
{ (void)f;(void)o;assert(net && inet && strstr(p,"/psp_vnet.prx"));return scenario==7 ? -1 : 42; }
int sceKernelStartModule(int m,unsigned n,void *a,int *status,void *o)
{
    (void)o;assert(m==42 && n==sizeof(vnet_start_args_t));
    vnet_start_args_t *args=a;assert(args->magic==VNET_START_MAGIC);
    if (scenario==8) { *status=-1;return -1; }
    vnet_start_result_t *r=(void *)(uintptr_t)args->result_address;
    r->result=scenario==9 ? -1 : 0;
    r->flags=scenario==9 ? 0 : VNET_REGISTERED|VNET_CALLBACK_OK|VNET_LAB_BACKEND;
    #ifdef VNET_USB_LAB
    r->flags |= VNET_USB_READY;
    gate_result=r;
    if(getenv("HTTP_GATE_TEST") || getenv("HTTP_CANCEL_TEST")) {
        const unsigned char ip[4]={203,0,113,7};memcpy(r->remote_ip,ip,4);
        r->flags |= VNET_UPLINK_READY;
        if(getenv("HTTP_CANCEL_TEST"))r->flags |= VNET_REMOTE_READY;
    }
    if (getenv("USB_GATE_TEST")) r->flags &= ~VNET_USB_READY;
#endif
    *status=0;started=1;return 42;
}
int sceKernelStopModule(int m,unsigned n,void *a,int *status,void *o)
{
    (void)o;assert(m==42 && started && !exited && !blocked_id);stops++;
    if ((scenario==10 || scenario==11 || scenario==15 || scenario==18) && stops==1) {
        assert(net && inet && !n && !a);*status=scenario==18 ? VNET_STOP_UNSAFE : (scenario==15 ? 0 : -1);
        if (scenario==15 || scenario==18) fatal=1;
        return scenario==15 ? -1 : 42;
    }
    if (stops==2) {
        assert(!net && !inet && net_terms==1);
        assert(n==sizeof(unsigned) && *(unsigned *)a==VNET_NETTERM_MAGIC);
    } else assert(!n && !a && net && inet);
    *status=0;started=0;return 42;
}
int sceKernelUnloadModule(int m)
{
    assert(m==42 && !started && !exited);driver_unloads++;
    if (scenario==14) { fatal=1;return -1; }
    return 0;
}
int sceCtrlPeekBufferPositive(SceCtrlData *p,int n)
{
    (void)n;assert(++polls<20);
    p->Buttons=scenario==0 ? PSP_CTRL_CIRCLE : (polls==1 ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE);
    if (scenario>=16 && polls==2) p->Buttons=PSP_CTRL_SQUARE;
    if (getenv("USB_GATE_TEST")) {
        if (polls==2) assert(!lab_calls);
        if (polls==3) { assert(!lab_calls); gate_result->flags |= VNET_USB_READY; p->Buttons=0; }
        if (polls==4) p->Buttons=PSP_CTRL_SQUARE;
    }
    if (getenv("HTTP_GATE_TEST")) {
        if(polls==2)p->Buttons=PSP_CTRL_TRIANGLE;
        if(polls==3)p->Buttons=PSP_CTRL_START;
        if(polls==4){assert(lab_calls==1);gate_result->flags|=VNET_REMOTE_READY;p->Buttons=0;}
        if(polls==5)p->Buttons=PSP_CTRL_START;
    }
    if(getenv("HTTP_CANCEL_TEST")) {
        if(polls==2)p->Buttons=PSP_CTRL_TRIANGLE;
        if(polls==3)p->Buttons=PSP_CTRL_START;
    }
    if ((scenario==20 || scenario==21) && polls>=3 && !aborts) p->Buttons=0;
    return 1;
}
int sceKernelDelayThreadCB(unsigned n)
{
    (void)n;sleeps++;
    if (fatal) longjmp(stopped,1);
    if (scenario==20 && blocked_id && !aborts) home(0,0,NULL);
    if (scenario==22 && aborts && polls>=5) longjmp(stopped,1);
    if (scenario==2) home(0,0,NULL);
    return 0;
}
void sceKernelExitGame(void)
{ assert(!common && !inet_module && !net && !inet && !started);exited++; }
int main(int argc,char **argv)
{
    assert(argc==2);scenario=atoi(argv[1]);
    char *args[]={"ms0:/PSP/GAME/psp-vnet/EBOOT.PBP",NULL};
    if (setjmp(stopped)==0) assert(vnet_loader_main(1,args)==0);
    if(scenario==22){
        assert(!exited && blocked_id && aborts==1 && queries && polls>=5);
        assert(started && !stops && !driver_unloads && net && inet);
        return 0;
    }
    if(scenario>=19){assert(exited==1 && aborts==1 && queries && !blocked_id && !abort_uid);}
    if (scenario==11 || scenario==12 || scenario==14 || scenario==15 || scenario==18) {
        assert(fatal && !exited && common && inet_module && net);
        if (scenario==11 || scenario==15 || scenario==18) assert(started && stops==1 && !driver_unloads);
        else assert(!started && driver_unloads==1);
    } else {
        assert(exited==1 && !fatal);
        if (scenario==10) assert(stops==2 && driver_unloads==1);
        if (scenario>=16) assert(lab_calls==((getenv("HTTP_GATE_TEST")||getenv("HTTP_CANCEL_TEST"))?2:1));
        if (scenario==0 || scenario==3 || scenario==4 || scenario==5 || scenario==13)
            assert(!stops && !net_terms);
        if (scenario==8) assert(!stops && driver_unloads==1);
    }
    return 0;
}

#ifdef VNET_USB_LAB
static int usb_owned;
int sceUtilityLoadUsbModule(int module) { assert(module==PSP_USB_MODULE_ACC && !usb_owned); usb_owned=1; return 0; }
int sceUtilityUnloadUsbModule(int module) { assert(module==PSP_USB_MODULE_ACC && usb_owned && !started); usb_owned=0; return 0; }
#endif

#ifdef VNET_USB_LAB
int vnet_run_native_http(int (*report)(const char *,int),const unsigned char ip[4])
{
    const unsigned char expected[4]={203,0,113,7};
    assert((getenv("HTTP_GATE_TEST") || getenv("HTTP_CANCEL_TEST")) && !memcmp(ip,expected,4));
    if(getenv("HTTP_GATE_TEST"))assert(polls>=5);
    lab_calls++;
    return report("HTTP_RECEIVE_WAIT",0)<0 ? -1 : 0;
}
#endif

#ifdef VNET_USB_LAB
int vnet_run_local_http(int (*report)(const char *,int))
{
    (void)report;assert(getenv("HTTP_GATE_TEST") || getenv("HTTP_CANCEL_TEST"));
    assert(!lab_calls);lab_calls++;return 0;
}
#endif
