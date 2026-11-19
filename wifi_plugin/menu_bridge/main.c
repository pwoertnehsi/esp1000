#include <pspkernel.h>
#include <pspiofilemgr.h>
#include "../menu_control.h"
#include "native_adapter.h"
PSP_MODULE_INFO("ESP WiFi XMB Bridge",0,0,25);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
extern int wifi_tls_init(void);
extern int wifi_tls_can_stop(void);
extern int wifiMenuRegister(unsigned,unsigned,unsigned,unsigned);
extern int wifiMenuRelease(void);
extern int wifiMenuOriginal(int,int);
static wifi_menu_control_t control={.magic=WIFI_MENU_MAGIC,.abi=WIFI_MENU_ABI};
static void exit_log(unsigned stage,unsigned phase,unsigned result)
{
    char line[112];unsigned used=0;
    const char *names[]={"BROWSER_EXIT stage="," phase="," result="," hooks="};
    unsigned values[]={stage,phase,result,(unsigned)control.exit_status};
    const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<4;++i) {
        const char *p=names[i];while(*p)line[used++]=*p++;
        for(unsigned j=0;j<8;++j)line[used++]=hex[(values[i]>>(28-4*j))&15];
    }
    line[used++]='\n';
    int fd=sceIoOpen("ms0:/browser-exit.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd>=0){sceIoWrite(fd,line,used);sceIoClose(fd);}
}
static int trace_exit(unsigned stage,unsigned a,unsigned b)
{
    exit_log(stage,0,0);
    int (*original)(unsigned,unsigned)=(void *)control.exit_originals[stage];
    int r=original?original(a,b):-1;
    exit_log(stage,1,(unsigned)r);return r;
}
#define EXIT_CALLBACK(n) static int exit_##n(unsigned a,unsigned b){return trace_exit(n,a,b);}
EXIT_CALLBACK(0) EXIT_CALLBACK(1) EXIT_CALLBACK(2) EXIT_CALLBACK(3) EXIT_CALLBACK(4) EXIT_CALLBACK(5) EXIT_CALLBACK(6) EXIT_CALLBACK(7) EXIT_CALLBACK(8)
static int execute(unsigned index,int action,int argument)
{
    control.last_action=action;control.last_argument=argument;
    __sync_synchronize();++control.calls;
    int (*original)(int,int)=(void *)control.original[index];
    return original?original(action,argument):-1;
}
static int execute0(int action,int argument){return execute(0,action,argument);}
static int execute1(int action,int argument){return execute(1,action,argument);}
static volatile int running;
static SceUID worker=-1;
static int browser_worker(SceSize size,void *args)
{
    (void)size;(void)args;
    while(running) {
        unsigned command=__sync_lock_test_and_set(&control.native_request,0);
        if(control.active && wifi_menu_uses_launcher(command)) {
            int (*original)(int,int)=(void *)control.original[0];
            if(original)original(command==WIFI_MENU_MANUAL?22:16,command==WIFI_MENU_MANUAL?2:0);
        }
        sceKernelDelayThread(50000);
    }
    return 0;
}
int module_start(SceSize size,void *args)
{(void)size;(void)args;
 control.original[0]=control.original[1]=(unsigned)wifiMenuOriginal;
 unsigned exits[]={(unsigned)exit_0,(unsigned)exit_1,(unsigned)exit_2,(unsigned)exit_3,(unsigned)exit_4,(unsigned)exit_5,(unsigned)exit_6,(unsigned)exit_7,(unsigned)exit_8};
 for(unsigned i=0;i<WIFI_EXIT_STAGES;++i)control.exit_callbacks[i]=exits[i];
 control.exit_owner=-1;
 int r=wifiMenuRegister((unsigned)&control,sizeof(control),(unsigned)execute0,(unsigned)execute1);
 if(r)return r;
 /* Keep the registered menu callbacks resident even if backend startup fails. */
 wifi_native_adapter_start();
 wifi_tls_init();
 worker=sceKernelCreateThread("ESPStockBrowser",browser_worker,0x30,0x10000,PSP_THREAD_ATTR_USER,NULL);
 if(worker<0)return 0;
 running=1;if(sceKernelStartThread(worker,0,NULL)<0){running=0;sceKernelDeleteThread(worker);worker=-1;}
 return 0;}
int module_stop(SceSize size,void *args)
{(void)size;(void)args;if(!wifi_tls_can_stop())return -2;int r=wifiMenuRelease();if(r)return r;
 running=0;if(worker>=0){unsigned timeout=2000000;if(sceKernelWaitThreadEnd(worker,&timeout)<0)return -2;sceKernelDeleteThread(worker);worker=-1;}
 return wifi_native_adapter_stop();}
