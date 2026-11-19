#include <assert.h>
#include <stdio.h>
#include "../wifi_plugin/main.c"
static int power_stop_allowed=1;
int wifi_power_can_stop(void){return power_stop_allowed;}
void wifi_menu_snapshot(wifi_menu_snapshot_t *out){memset(out,0,sizeof(*out));}
int wifi_menu_init(void){return 0;}
int wifi_menu_can_stop(void){return 1;}
int wifi_menu_stop(void){return 0;}
unsigned wifi_menu_poll(void){return 0;}
int wifi_menu_status(void){return 0;}
int wifi_menu_exit_status(void){return 0;}
int wifi_menu_verify(void){return 0;}
int wifi_menu_activity(unsigned out[3]){(void)out;return 0;}
void wifi_native_observed(unsigned out[4]){memset(out,0,16);}
unsigned wifi_native_detail(void){return 0;}
int wifi_native_status(void){return 0;}
int wifi_native_browser_status(void){return 0;}
unsigned wifi_native_browser_consumers(void){return 0;}
int wifi_native_tls_status(void){return 0;}
int wifi_native_init(void){return 0;}
int wifi_native_stop(void){return 0;}
static unsigned clock_us,buttons,devkit=0x06060110;
static int state,model=10,context=0x100,foreign,loops,launches;
static int create_result=3,start_result,wait_result,deleted,vsh_present=1,syscon_present;
static SceModule vsh,syscon;
static unsigned code[4];static unsigned char cached;
static unsigned log_opens,log_writes;static int write_limit=4096;
void prnt(void (*cb)(void *,int),void *ctx,const char *fmt,va_list args)
{char out[8192];int n=vsnprintf(out,sizeof(out),fmt,args);for(int i=0;i<n && i<(int)sizeof(out)-1;++i)cb(ctx,(unsigned char)out[i]);}
int sceIoOpen(const char *name,int flags,int mode)
{(void)flags;(void)mode;assert(!strcmp(name,"ms0:/wifi-vsh.log") && state>=0 && !(state&0x200));++log_opens;return 7;}
int sceIoWrite(int fd,const void *p,unsigned n)
{(void)p;assert(fd==7 && !(state&0x200));++log_writes;return n>(unsigned)write_limit?write_limit:(int)n;}
int sceIoClose(int fd){assert(fd==7);return 0;}
SceModule *sceKernelFindModuleByName(const char *name)
{if(!strcmp(name,"sceSYSCON_Driver"))return syscon_present?&syscon:NULL;return vsh_present?&vsh:NULL;}
unsigned sctrlHENFindFunction(const char *a,const char *b,unsigned nid)
{(void)a;(void)b;assert(nid==0xDF20C984);return (unsigned)(uintptr_t)code;}
int sceUsbGetState(void){return state;}
int sceUsbGetDrvState(const char *name){(void)name;return foreign?1:2;}
unsigned sceKernelGetSystemTimeLow(void){return clock_us;}
int sceCtrlPeekBufferPositive(SceCtrlData *pad,int n){assert(n==1);pad->Buttons=buttons;return 1;}
int sceKernelDelayThread(unsigned n){clock_us+=n;if(--loops<=0)running=0;return 0;}
int wifi_browser_init(void){return 0;}
int wifi_browser_stop(void){return 0;}
static int browser_status=1;
int wifi_browser_status(void){return browser_status;}
int wifi_menu_launch(unsigned command){assert(command==WIFI_MENU_BROWSER && !state && !foreign);++launches;return 0;}
unsigned sceKernelDevkitVersion(void){return devkit;}
int sceKernelGetModel(void){return model;}
int sceKernelInitKeyConfig(void){return context;}
int sceKernelCreateThread(const char *name,int (*fn)(SceSize,void *),int p,int s,int a,void *o)
{(void)name;(void)p;(void)s;(void)a;(void)o;assert(fn==monitor);return create_result;}
int sceKernelStartThread(int id,unsigned size,void *args){(void)id;(void)size;(void)args;return start_result;}
int sceKernelDeleteThread(int id){(void)id;++deleted;return 0;}
int sceKernelWaitThreadEnd(int id,unsigned *timeout){(void)id;assert(*timeout==2000000);return wait_result;}
static void run(unsigned pad,int usb,int busy,int ticks)
{buttons=pad;state=usb;foreign=busy;loops=ticks;running=1;monitor(0,NULL);}
int main(void)
{
 wifi_shortcut_t p={0};assert(!wifi_shortcut_update(&p,UINT32_MAX-10,1,1));
 assert(wifi_shortcut_update(&p,1000000,1,1));assert(!wifi_shortcut_update(&p,2000000,1,1));
 assert(!wifi_shortcut_update(&p,2000001,0,1));
 assert(!wifi_shortcut_update(&p,2000002,1,0));assert(!wifi_shortcut_update(&p,4000000,1,1));
 assert(!wifi_shortcut_update(&p,4000001,0,1));assert(!wifi_shortcut_update(&p,4000002,1,1));
 assert(wifi_shortcut_update(&p,5000002,1,1));
 model=0;assert(module_start(0,NULL)<0 && worker<0);model=10;
 context=0x200;assert(module_start(0,NULL)<0);context=0x100;
 devkit=0;assert(module_start(0,NULL)<0);devkit=0x06060110;
 create_result=-8;assert(module_start(0,NULL)==-8);create_result=3;
 start_result=-7;assert(module_start(0,NULL)==-7 && worker<0 && !running);start_result=0;
 devkit=0x06060010;assert(!module_start(0,NULL));assert(!module_stop(0,NULL));devkit=0x06060110;
 assert(!module_start(0,NULL));power_stop_allowed=0;assert(module_stop(0,NULL)==-2 && worker>=0 && running);power_stop_allowed=1;wait_result=-1;assert(module_stop(0,NULL)<0 && worker>=0);
 wait_result=0;assert(!module_stop(0,NULL) && worker<0);
 run(CHORD,0x200,0,30);run(CHORD,0,1,30);run(PSP_CTRL_SQUARE,0,0,30);assert(!launches);
 run(CHORD,0,0,60);assert(launches==1); /* Holding never repeatedly launches. */
 state=0x200;assert(launch()==-2 && launches==1);state=0;
 browser_status=-1;assert(launch()==-3 && launches==1);browser_status=1;
 /* Optional detector is guarded and cannot turn an unrecognized firmware
  * routine into a hardware write or a guessed accessory-ID decision. */
 syscon_present=1;strcpy(syscon.modname,"sceSYSCON_Driver");syscon.version[0]=11;syscon.version[1]=1;
 syscon.text_size=19980;syscon.text_addr=(unsigned)(uintptr_t)code-0x21d0;
 unsigned data=(unsigned)(uintptr_t)&cached;
 code[0]=0x3c020000u|((data+0x8000u)>>16);code[1]=0x90440000u|(data&0xffffu);
 code[2]=0x03e00008u;code[3]=0x7c820100u;syscon.nsegment=1;
 syscon.segmentaddr[0]=data;syscon.segmentsize[0]=1;
 resolve_passive_flag();assert(usb_flag!=NULL);usb_flag=NULL;
 code[3]^=1;resolve_passive_flag();assert(!usb_flag);code[3]^=1;
 syscon.segmentaddr[0]=data+1;resolve_passive_flag();assert(!usb_flag);
 for(unsigned i=0;i<1000;++i)remember(i,(int)i,0,0);
 diagnostic("test=%d\n",17);state=0x222;unsigned before=log_opens;
 flush_diagnostics();assert(log_opens==before && diagnostic_pending);
 state=-1;flush_diagnostics();assert(log_opens==before);
 state=0;write_limit=2;unsigned bytes=diagnostic_size;
 flush_diagnostics();assert(diagnostic_size==bytes-2 && diagnostic_pending);
 write_limit=4096;flush_diagnostics();assert(!diagnostic_pending && log_writes);
 before=log_opens;flush_diagnostics();assert(before==log_opens);
 puts("PASS: real VSH plugin guards/lifecycle, storage refusal, hold/release/wrap, resident browser request, passive detector guards");
}
