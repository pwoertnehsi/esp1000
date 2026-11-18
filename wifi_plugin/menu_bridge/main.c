#include <pspkernel.h>
#include "../menu_control.h"
#include "native_adapter.h"
PSP_MODULE_INFO("ESP WiFi XMB Bridge",0,0,8);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
extern int wifiMenuRegister(unsigned,unsigned,unsigned,unsigned);
extern int wifiMenuRelease(void);
extern int wifiMenuOriginal(int,int);
static wifi_menu_control_t control={.magic=WIFI_MENU_MAGIC,.abi=WIFI_MENU_ABI};
static int execute(unsigned index,int action,int argument)
{
    control.last_action=action;control.last_argument=argument;
    __sync_synchronize();++control.calls;
    unsigned command=wifi_menu_command(action,argument);
    if(control.active && wifi_menu_uses_launcher(command)) {
        __sync_bool_compare_and_swap(&control.pending,0,command);
        return 0;
    }
    int (*original)(int,int)=(void *)control.original[index];
    return original?original(action,argument):-1;
}
static int execute0(int action,int argument){return execute(0,action,argument);}
static int execute1(int action,int argument){return execute(1,action,argument);}
int module_start(SceSize size,void *args)
{(void)size;(void)args;
 control.original[0]=control.original[1]=(unsigned)wifiMenuOriginal;
 int r=wifiMenuRegister((unsigned)&control,sizeof(control),(unsigned)execute0,(unsigned)execute1);
 if(r)return r;
 /* Keep the registered menu callbacks resident even if backend startup fails. */
 wifi_native_adapter_start();return 0;}
int module_stop(SceSize size,void *args)
{(void)size;(void)args;int r=wifiMenuRelease();return r?r:wifi_native_adapter_stop();}
