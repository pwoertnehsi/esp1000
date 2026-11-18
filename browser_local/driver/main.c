#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsdk.h>
#include <systemctrl.h>
#include <string.h>
#include "../control.h"
#include "path_patch.h"
#include "code_patch.h"
#include "asset_device.h"
#include "apctl_guards.h"
PSP_MODULE_INFO("Browser RAM Paths",PSP_MODULE_KERNEL,0,6);
typedef struct {unsigned offset,expected;} browser_anchor_t;
typedef struct {
    const char *name;unsigned minor,major,size;
    const browser_path_t *paths;unsigned count;
    const browser_anchor_t *anchors;unsigned anchor_count;
} browser_guard_t;
#include "browser_guards.h"
#include "browser_gate_guards.h"
static volatile browser_status_t *status;
static STMOD_HANDLER previous;
static volatile unsigned in_handler;
static int installed;
static SceUID owners[2]={-1,-1};
static unsigned owner_addresses[2];
static browser_code_patch_t apctl_patches[6];
static SceUID apctl_owner=-1;
static unsigned apctl_address;
static int apctl_installed;
static int prepare_apctl(const browser_start_t *request)
{
    if(!request->callback_module)return 0;
    SceModule *app=sceKernelFindModuleByUID(request->callback_module);
    SceModule *ap=sceKernelFindModuleByName("sceNetApctl_Library");
    if(!app || !ap || ap->text_size!=BROWSER_APCTL_SIZE || ap->version[0]!=3 || ap->version[1]!=1)return -1;
    for(unsigned i=0;i<6;++i) {
        unsigned target=request->callbacks[i];
        unsigned address=sctrlHENFindFunction("sceNetApctl_Library","sceNetApctl",apctl_entries[i].nid);
        if(address!=ap->text_addr+apctl_entries[i].offset || target<0x08800000 || target>=0x0c000000
           || (target&3) || target<app->text_addr || target-app->text_addr>=app->text_size
           || ((address+4)^target)&0xf0000000)return -1;
        uint32_t *code=(void *)address;
        if(code[0]!=apctl_entries[i].first || code[1]!=apctl_entries[i].second)return -1;
        apctl_patches[i]=(browser_code_patch_t){code,{code[0],code[1]},
            {0x08000000u|((target>>2)&0x03ffffffu),0}};
    }
    apctl_owner=ap->modid;apctl_address=ap->text_addr;return 0;
}
static int change_apctl(int restore)
{
    if(apctl_owner<0)return 0;
    SceModule *m=sceKernelFindModuleByUID(apctl_owner);
    if(!m || m->text_addr!=apctl_address)return -1;
    int interrupt=sceKernelCpuSuspendIntr();
    int r=browser_code_change(apctl_patches,6,restore);
    if(!r)for(unsigned i=0;i<6;++i) {
        sceKernelDcacheWritebackRange(apctl_patches[i].address,8);
        sceKernelIcacheInvalidateRange(apctl_patches[i].address,8);
    }
    sceKernelCpuResumeIntr(interrupt);
    if(!r){apctl_installed=!restore;status->apctl_hooks=restore?0:6;}
    return r;
}
static int change(SceModule *m,unsigned index,int restore)
{
    const browser_guard_t *g=&browser_guards[index];
    if(strcmp(m->modname,g->name) || m->version[0]!=g->minor || m->version[1]!=g->major
       || m->text_size!=g->size || !m->nsegment || m->nsegment>4
       || m->text_addr!=m->segmentaddr[0] || m->segmentsize[0]<g->size)return -1;
    for(unsigned i=0;i<g->anchor_count;++i)
        if(*(volatile unsigned *)(m->text_addr+g->anchors[i].offset)!=g->anchors[i].expected)return -1;
    browser_code_patch_t gates[4];
    int gate_count=index==1 && apctl_owner>=0?4:0;
    int interrupt=sceKernelCpuSuspendIntr();
    for(int i=0;i<gate_count;++i) {
        const unsigned offset=browser_gate_entries[i].offset;
        uint32_t *code=(void *)(m->text_addr+offset);
        gates[i]=(browser_code_patch_t){code,{browser_gate_entries[i].branch,browser_gate_entries[i].delay},
                                      {0,browser_gate_entries[i].delay}};
        const uint32_t *expected=restore?gates[i].replacement:gates[i].original;
        if(code[0]!=expected[0] || code[1]!=expected[1]) {
            sceKernelCpuResumeIntr(interrupt);return -1;
        }
    }
    int r=browser_paths_change((void *)m->segmentaddr[0],g->size,g->paths,g->count,restore);
    if(!r)for(unsigned i=0;i<g->count;++i)
        sceKernelDcacheWritebackRange((void *)(m->segmentaddr[0]+g->paths[i].offset),8);
    if(!r && gate_count) {
        r=browser_code_change(gates,gate_count,restore);
        if(!r)for(int i=0;i<gate_count;++i) {
            sceKernelDcacheWritebackRange(gates[i].address,8);
            sceKernelIcacheInvalidateRange(gates[i].address,8);
        }
        if(!r)status->browser_gates=restore?0:gate_count;
    }
    sceKernelCpuResumeIntr(interrupt);
    return r;
}
static int observe(SceModule *m)
{
    static const char *const names[]={"sceUtility_Driver","sceVshHVUtility_Module","sceHVUI_Module",
        "sceHVNetfront_Module","sceHVSlim_Library","sceHVWWW_Library","sceHVFlash_Module"};
    for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i)
        if(!strcmp(m->modname,names[i]))status->module_mask|=1u<<i;
    for(unsigned i=0;i<2;++i) {
        if(strcmp(m->modname,browser_guards[i].name))continue;
        status->observed++;
        status->last_uid=m->modid;
        if(owners[i]==m->modid && owner_addresses[i]==m->text_addr)return 0;
        if(owners[i]>=0 && sceKernelFindModuleByUID(owners[i])) {
            status->refused++;status->last_result=-1;return -1;
        }
        int r=change(m,i,0);status->last_result=r;
        if(r<0) {status->refused++;return r;}
        owners[i]=m->modid;owner_addresses[i]=m->text_addr;
        status->patches+=browser_guards[i].count;
        return 0;
    }
    return 0;
}
static int on_start(SceModule *m)
{
    int interrupt=sceKernelCpuSuspendIntr();++in_handler;sceKernelCpuResumeIntr(interrupt);
    /* Chain CFW/plugin work first. Only exact verified browser constants are
     * then redirected. APCTL hooks are installed separately for this session. */
    int r=previous?previous(m):0;
    observe(m);
    interrupt=sceKernelCpuSuspendIntr();--in_handler;sceKernelCpuResumeIntr(interrupt);
    return r;
}
int module_start(SceSize size,void *args)
{
    browser_start_t request;
    if(size!=sizeof(request) || !args)return -1;
    memcpy(&request,args,sizeof(request));
    if(request.magic!=BROWSER_START_MAGIC || (request.result_address&3)
       || request.result_address<0x08800000 || request.result_address>0x0c000000-sizeof(browser_status_t))return -1;
    status=(void *)request.result_address;
    memset((void *)status,0,sizeof(*status));status->result=-1;
    if(sceKernelDevkitVersion()!=0x06060110 || sceKernelGetModel()!=10)return 0;
    /* Do not interfere with an already-active viewer or another runtime owner. */
    if(sceKernelFindModuleByName("sceVshHVUtility_Module") || sceKernelFindModuleByName("sceHVUI_Module"))return 0;
    SceModule *utility=sceKernelFindModuleByName("sceUtility_Driver");
    if(prepare_apctl(&request)<0 || !utility)return 0;
    if(browser_asset_device_start(status)<0)return 0;
    if(observe(utility)<0)return 0;
    if(request.callback_module && change_apctl(0)<0)return 0;
    int interrupt=sceKernelCpuSuspendIntr();
    previous=sctrlHENSetStartModuleHandler(on_start);installed=1;
    sceKernelCpuResumeIntr(interrupt);
    status->active=1;status->result=0;
    return 0;
}
int module_stop(SceSize size,void *args)
{
    (void)size;(void)args;
    if(!status)return 0;
    int interrupt=sceKernelCpuSuspendIntr();
    if(in_handler) {sceKernelCpuResumeIntr(interrupt);return -2;}
    if(installed) {
        STMOD_HANDLER current=sctrlHENSetStartModuleHandler(previous);
        if(current!=on_start) {
            sctrlHENSetStartModuleHandler(current);
            sceKernelCpuResumeIntr(interrupt);return -2;
        }
        installed=0;
    }
    sceKernelCpuResumeIntr(interrupt);
    if(apctl_installed && change_apctl(1)<0)return -2;
    for(unsigned i=0;i<2;++i) {
        if(owners[i]<0)continue;
        SceModule *m=sceKernelFindModuleByUID(owners[i]);
        if(m && (m->text_addr!=owner_addresses[i] || change(m,i,1)<0)) {
            status->restore_fail++;return -2;
        }
        owners[i]=-1;
    }
    if(browser_asset_device_stop()<0)return -2;
    status->active=0;
    return 0;
}
