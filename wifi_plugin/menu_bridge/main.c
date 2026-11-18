#include <pspkernel.h>
#include "../menu_control.h"
#include "native_adapter.h"
PSP_MODULE_INFO("ESP WiFi XMB Bridge",0,0,19);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
extern int wifi_tls_init(void);
extern int wifi_tls_can_stop(void);
extern int wifiMenuRegister(unsigned,unsigned,unsigned,unsigned);
extern int wifiMenuRelease(void);
extern int wifiMenuOriginal(int,int);
static wifi_menu_control_t control={.magic=WIFI_MENU_MAGIC,.abi=WIFI_MENU_ABI};
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
