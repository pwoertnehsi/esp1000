#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../browser_local/app/network_menu.c"
static unsigned presses[40],position,count;
static char output[20000];static unsigned used;
void pspDebugScreenClear(void){}
int pspDebugScreenPrintf(const char *format,...)
{va_list ap;va_start(ap,format);int n=vsnprintf(output+used,sizeof(output)-used,format,ap);va_end(ap);assert(n>=0 && (unsigned)n<sizeof(output)-used);used+=(unsigned)n;return n;}
int sceCtrlPeekBufferPositive(SceCtrlData *pad,int size){assert(size==1 && position<count);pad->Buttons=presses[position++];return 1;}
int sceDisplayWaitVblankStart(void){return 0;}
int sceKernelDelayThreadCB(unsigned us){assert(us==20000);return 0;}
static void key(unsigned key){presses[count++]=key;presses[count++]=0;}
static void reset(void){used=position=count=0;output[0]=0;presses[count++]=0;}
int main(void)
{
 volatile int exiting=0;
 assert(network_menu(WIFI_MENU_BROWSER,&exiting)==1);
 assert(network_menu(WIFI_MENU_MANUAL,&exiting)==1);
 reset();key(PSP_CTRL_CROSS);key(PSP_CTRL_DOWN);key(PSP_CTRL_CROSS);key(PSP_CTRL_CIRCLE);key(PSP_CTRL_CIRCLE);key(PSP_CTRL_CIRCLE);
 assert(!network_menu(WIFI_MENU_SETTINGS,&exiting));
 assert(strstr(output,"Ad Hoc Mode") && strstr(output,"Infrastructure Mode"));
 assert(strstr(output,"Automatic") && strstr(output,"Ch 1") && strstr(output,"Ch 6") && strstr(output,"Ch 11"));
 assert(strstr(output,"Channel was not saved or applied"));
 reset();key(PSP_CTRL_DOWN);key(PSP_CTRL_CROSS);key(PSP_CTRL_CROSS);key(PSP_CTRL_CROSS);
 assert(network_menu(WIFI_MENU_SETTINGS,&exiting)==1 && strstr(output,"ESP adapter"));
 reset();key(PSP_CTRL_DOWN);key(PSP_CTRL_CROSS);key(PSP_CTRL_CROSS);key(PSP_CTRL_DOWN);key(PSP_CTRL_CROSS);
 assert(network_menu(WIFI_MENU_SETTINGS,&exiting)==2);
 reset();key(PSP_CTRL_DOWN);key(PSP_CTRL_CROSS);key(PSP_CTRL_DOWN);key(PSP_CTRL_CROSS);key(PSP_CTRL_CROSS);
 key(PSP_CTRL_CIRCLE);key(PSP_CTRL_CIRCLE);key(PSP_CTRL_CIRCLE);key(PSP_CTRL_CIRCLE);
 assert(!network_menu(WIFI_MENU_SETTINGS,&exiting));
 assert(strstr(output,"Scan") && strstr(output,"Enter Manually") && strstr(output,"No connection or password was saved"));
 reset();key(PSP_CTRL_CIRCLE);assert(!network_menu(WIFI_MENU_GAME_SHARING,&exiting));assert(strstr(output,"ad hoc transport"));
 puts("Settings mode separation, channel menu, browser/test routing and honest unsupported states passed");
 return 0;
}
