#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "../wifi_plugin/menu_bridge/main.c"
int wifi_native_adapter_start(void){return 0;}
int wifi_native_adapter_stop(void){return 0;}
static int registrations,releases,original_calls,last_action,last_argument;
int wifiMenuOriginal(int action,int argument){++original_calls;last_action=action;last_argument=argument;return 37;}
int wifiMenuRegister(unsigned address,unsigned size,unsigned first,unsigned second)
{
 assert(address==(unsigned)(uintptr_t)&control && size==sizeof(control));
 assert(control.abi==2 && control.original[0]==(unsigned)(uintptr_t)wifiMenuOriginal && control.original[1]==control.original[0]);
 assert(first==(unsigned)(uintptr_t)execute0 && second==(unsigned)(uintptr_t)execute1);
 ++registrations;return 0;
}
int wifiMenuRelease(void){++releases;return control.active?-2:0;}
int main(void)
{
 assert(!module_start(0,NULL) && registrations==1);
 control.active=1;
 assert(execute0(3,3)==37 && !control.pending && original_calls==1);
 assert(control.calls==1 && control.last_action==3 && control.last_argument==3);
 assert(execute1(12,0)==37 && !control.pending && original_calls==2);
 assert(execute0(2,9)==37 && !control.pending && original_calls==3);
 assert(!execute0(16,0) && control.pending==WIFI_MENU_BROWSER && original_calls==3);
 assert(!execute1(22,2) && control.pending==WIFI_MENU_BROWSER);
 control.pending=0;assert(!execute1(22,2) && control.pending==WIFI_MENU_MANUAL);
 assert(execute0(2,123)==37 && original_calls==4 && last_action==2 && last_argument==123);
 assert(module_stop(0,NULL)==-2 && releases==1);
 control.active=0;control.pending=0;
 assert(execute1(3,3)==37 && original_calls==5 && !control.pending);
 assert(!module_stop(0,NULL) && releases==2);
 puts("Actual user bridge ABI, action interception, queue bounds, original fallback and unload refusal passed");
 return 0;
}
