#include "browser_helper_shim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../wifi_plugin/browser_runtime.c"
static unsigned char utility_bytes[0xb038],plugin_bytes[0x1f700];
static SceModule utility,plugin;
static STMOD_HANDLER current;
static int plugin_present,device,previous_calls;
int sceKernelCpuSuspendIntr(void){return 0;}
void sceKernelCpuResumeIntr(int i){(void)i;}
void sceKernelDcacheWritebackRange(const void *p,unsigned n){assert(p && n==8);}
SceModule *sceKernelFindModuleByName(const char *name)
{return !strcmp(name,"sceUtility_Driver")?&utility:!strcmp(name,"htmlviewer_plugin_module") && plugin_present?&plugin:NULL;}
SceModule *sceKernelFindModuleByUID(int id){return id==1?&utility:id==2 && plugin_present?&plugin:NULL;}
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER next){STMOD_HANDLER old=current;current=next;return old;}
int browser_asset_device_start(volatile browser_status_t *s){(void)s;assert(!device);device=1;return 0;}
int browser_asset_device_stop(void){device=0;return 0;}
static int prior(SceModule *m){assert(m==&plugin);++previous_calls;return 17;}
static void fill(SceModule *m,unsigned char *data,unsigned i,int id)
{
    const browser_guard_t *g=guard(i);
    strcpy(m->modname,g->name);m->version[0]=g->minor;m->version[1]=g->major;m->modid=id;m->nsegment=1;
    m->text_addr=m->segmentaddr[0]=(unsigned)(uintptr_t)data;m->text_size=m->segmentsize[0]=g->size;
    for(unsigned j=0;j<g->count;++j)strcpy((char *)data+g->paths[j].offset,g->paths[j].original);
    for(unsigned j=0;j<g->anchor_count;++j)memcpy(data+g->anchors[j].offset,&g->anchors[j].expected,4);
}
int main(void)
{
    fill(&utility,utility_bytes,0,1);fill(&plugin,plugin_bytes,1,2);current=prior;
    utility_bytes[browser_paths_0[4].offset]='X';
    assert(wifi_browser_init()<0 && !device && current==prior);
    assert(!strcmp((char *)utility_bytes+browser_paths_0[0].offset,browser_paths_0[0].original));
    utility_bytes[browser_paths_0[4].offset]='f';
    assert(!wifi_browser_init() && device && current==on_start);
    assert(!strcmp((char *)utility_bytes+browser_paths_0[0].offset,browser_paths_0[0].redirected));
    plugin_present=1;plugin_bytes[plugin_paths[0].offset]='X';
    assert(current(&plugin)==17 && previous_calls==1 && status<0);
    plugin_bytes[plugin_paths[0].offset]='f';
    assert(current(&plugin)==17 && status==1);
    assert(!strcmp((char *)plugin_bytes+plugin_paths[0].offset,plugin_paths[0].redirected));
    assert(wifi_browser_stop()==-2 && device && current==on_start);
    plugin_present=0;
    assert(!wifi_browser_stop() && !device && current==prior);
    assert(!strcmp((char *)utility_bytes+browser_paths_0[0].offset,browser_paths_0[0].original));
    puts("PASS: resident browser asset preflight, chained native-plugin loading and live-unload refusal");
}
