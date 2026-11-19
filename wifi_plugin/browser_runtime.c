/* Restore missing browser assets for Sony's VSH plugin. Sony owns graphics,
 * parental controls and the browser lifecycle; no utility render loop or EBOOT. */
#include <pspkernel.h>
#include <psploadcore.h>
#include <systemctrl.h>
#include <string.h>
#include "browser_runtime.h"
#include "../browser_local/driver/path_patch.h"
#include "../browser_local/driver/asset_device.h"
typedef struct {unsigned offset,expected;} browser_anchor_t;
typedef struct {
    const char *name;unsigned minor,major,size;
    const browser_path_t *paths;unsigned count;
    const browser_anchor_t *anchors;unsigned anchor_count;
} browser_guard_t;
#include "../browser_local/driver/browser_guards.h"
static const browser_path_t plugin_paths[]={
    {0x1c534,"flash0:/vsh/resource/","brows0:/vsh/resource/"}
};
static const browser_anchor_t plugin_anchors[]={
    {0,0x27bdffe0},{0x133e8,0x27bdfee0},{0x18b88,0x27bdfff0}
};
static const browser_guard_t plugin_guard={"htmlviewer_plugin_module",1,1,0x1f700,
    plugin_paths,1,plugin_anchors,3};
static STMOD_HANDLER previous;
static int installed,status;
static SceUID owners[2]={-1,-1};
static unsigned addresses[2];
static browser_status_t device_status;
static const browser_guard_t *guard(unsigned i){return i?&plugin_guard:&browser_guards[0];}
static int change(SceModule *m,unsigned index,int restore)
{
    const browser_guard_t *g=guard(index);
    if(!m || strcmp(m->modname,g->name) || m->version[0]!=g->minor || m->version[1]!=g->major || m->text_size!=g->size ||
        !m->nsegment || m->nsegment>4 || m->text_addr!=m->segmentaddr[0] || m->segmentsize[0]<g->size)return -1;
    for(unsigned i=0;i<g->anchor_count;++i)
        if(*(unsigned *)(m->text_addr+g->anchors[i].offset)!=g->anchors[i].expected)return -2;
    int r=browser_paths_change((void *)m->text_addr,g->size,g->paths,g->count,restore);
    if(!r)for(unsigned i=0;i<g->count;++i)sceKernelDcacheWritebackRange((void *)(m->text_addr+g->paths[i].offset),8);
    return r;
}
static void observe(SceModule *m)
{
    if(!m)return;
    for(unsigned i=0;i<2;++i) {
        if(strcmp(m->modname,guard(i)->name))continue;
        if(owners[i]==m->modid && addresses[i]==m->text_addr)return;
        if(owners[i]>=0 && sceKernelFindModuleByUID(owners[i])){status=-3;return;}
        int r=change(m,i,0);
        if(r){status=r;return;}
        owners[i]=m->modid;addresses[i]=m->text_addr;status=1;
    }
}
static int on_start(SceModule *m)
{
    int r=previous?previous(m):0;
    int intr=sceKernelCpuSuspendIntr();observe(m);sceKernelCpuResumeIntr(intr);return r;
}
int wifi_browser_init(void)
{
    if(installed)return -1;
    if(sceKernelFindModuleByName(plugin_guard.name) || sceKernelFindModuleByName("sceHVUI_Module"))return -1;
    if(browser_asset_device_start(&device_status)<0)return status=-4;
    observe(sceKernelFindModuleByName("sceUtility_Driver"));
    if(status!=1){browser_asset_device_stop();return -1;}
    int intr=sceKernelCpuSuspendIntr();previous=sctrlHENSetStartModuleHandler(on_start);installed=1;sceKernelCpuResumeIntr(intr);
    return 0;
}
int wifi_browser_stop(void)
{
    if(!installed)return 0;
    if(sceKernelFindModuleByName(plugin_guard.name) || sceKernelFindModuleByName("sceHVUI_Module"))return -2;
    int intr=sceKernelCpuSuspendIntr();
    STMOD_HANDLER current=sctrlHENSetStartModuleHandler(previous);
    if(current!=on_start){sctrlHENSetStartModuleHandler(current);sceKernelCpuResumeIntr(intr);return -2;}
    for(unsigned i=0;i<2;++i)if(owners[i]>=0) {
        SceModule *m=sceKernelFindModuleByUID(owners[i]);
        if(m && (m->text_addr!=addresses[i] || change(m,i,1)<0)) {
            sctrlHENSetStartModuleHandler(on_start);sceKernelCpuResumeIntr(intr);return -2;
        }
    }
    sceKernelCpuResumeIntr(intr);
    if(browser_asset_device_stop()<0){sctrlHENSetStartModuleHandler(on_start);return -2;}
    installed=0;return 0;
}
int wifi_browser_status(void){return status;}
