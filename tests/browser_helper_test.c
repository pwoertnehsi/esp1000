#define _GNU_SOURCE
#include "browser_helper_shim.h"
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../browser_local/driver/main.c"
static unsigned char utility_bytes[0xb038] __attribute__((aligned(16)));
static unsigned char viewer_bytes[0xe808] __attribute__((aligned(16)));
static SceModule utility,wrapper,apctl_module,app_module;
static unsigned char *apctl_bytes;
static int scenario,wrapper_present,previous_calls;
static STMOD_HANDLER current;
int browser_asset_device_start(volatile browser_status_t *s){(void)s;return 0;}
int browser_asset_device_stop(void){return 0;}
static int next_handler(SceModule *m){(void)m;return 23;}
static int old_handler(SceModule *m)
{
    previous_calls++;
    if(m==&wrapper)assert(!strcmp((char *)viewer_bytes+browser_paths_1[0].offset,browser_paths_1[0].original));
    if(scenario==10)assert(module_stop(0,NULL)==-2);
    return 17;
}
int sceKernelCpuSuspendIntr(void){return 0;}
void sceKernelCpuResumeIntr(int i){(void)i;}
void sceKernelDcacheWritebackRange(const void *p,unsigned n){assert(p && n==8);}
void sceKernelIcacheInvalidateRange(const void *p,unsigned n){assert(p && n==8);}
unsigned sctrlHENFindFunction(const char *m,const char *l,unsigned n){(void)m;(void)l;for(unsigned i=0;i<6;++i)if(n==apctl_entries[i].nid)return apctl_module.text_addr+apctl_entries[i].offset;return 0;}
unsigned sceKernelDevkitVersion(void){return scenario==1?0x06060010:0x06060110;}
int sceKernelGetModel(void){return 10;}
SceModule *sceKernelFindModuleByUID(int uid)
{return uid==101?&utility:uid==103?&apctl_module:uid==104?&app_module:uid==102 && wrapper_present?&wrapper:NULL;}
SceModule *sceKernelFindModuleByName(const char *name)
{
    if(!strcmp(name,"sceNetApctl_Library") && scenario>=11)return &apctl_module;
    if(!strcmp(name,"sceUtility_Driver"))return &utility;
    if(scenario==8 && !strcmp(name,"sceHVUI_Module"))return &wrapper;
    return NULL;
}
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER h){STMOD_HANDLER old=current;current=h;return old;}
static void fill(SceModule *m,unsigned char *data,unsigned index,int uid)
{
    const browser_guard_t *g=&browser_guards[index];
    strcpy(m->modname,g->name);m->version[0]=g->minor;m->version[1]=g->major;m->nsegment=1;
    m->text_addr=m->segmentaddr[0]=(unsigned)(uintptr_t)data;
    m->text_size=m->segmentsize[0]=g->size;m->modid=uid;
    for(unsigned i=0;i<g->count;++i)strcpy((char *)data+g->paths[i].offset,g->paths[i].original);
    if(index==1)for(unsigned i=0;i<4;++i){memcpy(data+browser_gate_entries[i].offset,&browser_gate_entries[i].branch,4);memcpy(data+browser_gate_entries[i].offset+4,&browser_gate_entries[i].delay,4);}
    for(unsigned i=0;i<g->anchor_count;++i)memcpy(data+g->anchors[i].offset,&g->anchors[i].expected,4);
}
int main(int argc,char **argv)
{
    assert(argc==2);scenario=atoi(argv[1]);fill(&utility,utility_bytes,0,101);fill(&wrapper,viewer_bytes,1,102);
    current=old_handler;
    void *result=mmap((void *)0x08900000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(result==(void *)0x08900000);
    if(scenario==2)utility_bytes[browser_paths_0[4].offset]='X';
    if(scenario==7)utility_bytes[browser_anchors_0[0].offset]^=1;
    browser_start_t request={.magic=BROWSER_START_MAGIC,.result_address=(unsigned)(uintptr_t)result};
    if(scenario>=11) {
        apctl_bytes=mmap((void *)0x0af00000,0x20000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        assert(apctl_bytes==(void *)0x0af00000);
        apctl_module.text_addr=0x0af00000;apctl_module.text_size=BROWSER_APCTL_SIZE;
        apctl_module.version[0]=3;apctl_module.version[1]=1;apctl_module.modid=103;
        app_module.text_addr=0x0b000000;app_module.text_size=0x10000;app_module.modid=104;
        request.callback_module=104;
        for(unsigned i=0;i<6;++i) {
            request.callbacks[i]=app_module.text_addr+i*16;
            uint32_t *code=(void *)(apctl_bytes+apctl_entries[i].offset);
            code[0]=apctl_entries[i].first;code[1]=apctl_entries[i].second;
        }
        if(scenario==12)request.callbacks[5]=0x08800000;
        if(scenario==13)apctl_bytes[apctl_entries[5].offset]^=1;
        if(scenario==14)apctl_module.version[0]=2;
    }
    assert(module_start(sizeof(request),&request)==0);
    if(scenario==1 || scenario==2 || scenario==7 || scenario==8 || scenario==12 || scenario==13 || scenario==14) {
        assert(status->result<0 && !status->active && current==old_handler);
        assert(!strcmp((char *)utility_bytes+browser_paths_0[0].offset,browser_paths_0[0].original));
        assert(!module_stop(0,NULL));return 0;
    }
    assert(!status->result && status->active && status->patches==5 && current==on_start);
    if(scenario>=11) {
        assert(status->apctl_hooks==6);
        for(unsigned i=0;i<6;++i)assert(*(uint32_t *)(apctl_bytes+apctl_entries[i].offset)==(0x08000000u|((request.callbacks[i]>>2)&0x03ffffffu)));

    }
    wrapper_present=1;
    if(scenario==3)viewer_bytes[browser_anchors_1[0].offset]^=1;
    if(scenario==16)viewer_bytes[browser_gate_entries[3].offset]^=1;
    assert(current(&wrapper)==17 && previous_calls==1);
    if(scenario==3 || scenario==16)assert(status->refused==1 && status->patches==5);
    else assert(status->patches==6 && !strcmp((char *)viewer_bytes+browser_paths_1[0].offset,browser_paths_1[0].redirected));
        if(scenario==15) {
            uint32_t *last=(void *)(apctl_bytes+apctl_entries[5].offset);uint32_t saved=*last;*last^=1;
            assert(module_stop(0,NULL)==-2 && status->apctl_hooks==6);
            assert(*(uint32_t *)(apctl_bytes+apctl_entries[0].offset)==apctl_patches[0].replacement[0]);*last=saved;
        }
    if(scenario==9){previous=NULL;assert(current(&wrapper)==0 && status->patches==6);previous=old_handler;}
    if(scenario==4){viewer_bytes[browser_paths_1[0].offset]='X';assert(module_stop(0,NULL)==-2 && current==old_handler);strcpy((char *)viewer_bytes+browser_paths_1[0].offset,browser_paths_1[0].redirected);}
    if(scenario==5){current=next_handler;assert(module_stop(0,NULL)==-2 && current==next_handler);current=on_start;}
    if(scenario==6)wrapper_present=0;
    if(scenario>=11 && scenario!=12 && scenario!=13 && scenario!=14 && scenario!=16) {
        assert(status->browser_gates==4);
        for(unsigned i=0;i<4;++i)assert(!*(uint32_t *)(viewer_bytes+browser_gate_entries[i].offset));
    }
    if(scenario==17) {
        uint32_t *code=(void *)(viewer_bytes+browser_gate_entries[3].offset);*code=1;
        assert(module_stop(0,NULL)==-2);
        assert(!strcmp((char *)viewer_bytes+browser_paths_1[0].offset,browser_paths_1[0].redirected));
        assert(!*(uint32_t *)(viewer_bytes+browser_gate_entries[0].offset));*code=0;
    }
    assert(!module_stop(0,NULL) && current==old_handler && !status->active);
    if(scenario>=11 && scenario!=16 && wrapper_present)for(unsigned i=0;i<4;++i)assert(*(uint32_t *)(viewer_bytes+browser_gate_entries[i].offset)==browser_gate_entries[i].branch);
    if(scenario>=11)for(unsigned i=0;i<6;++i)assert(*(uint32_t *)(apctl_bytes+apctl_entries[i].offset)==apctl_entries[i].first);
    for(unsigned i=0;i<5;++i)assert(!strcmp((char *)utility_bytes+browser_paths_0[i].offset,browser_paths_0[i].original));
    if(wrapper_present)assert(!strcmp((char *)viewer_bytes+browser_paths_1[0].offset,browser_paths_1[0].original));
}
