#define _GNU_SOURCE
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include "../wifi_plugin/native_runtime.c"
static SceModule netconf,bridge,api,browser;
static STMOD_HANDLER handler;
static int present=1,caches,prior_calls,api_present=1,browser_present;
unsigned pspSdkSetK1(unsigned k){return k;}
int sceKernelCpuSuspendIntr(void){return 1;}
void sceKernelCpuResumeIntr(int n){assert(n==1);}
void sceKernelDcacheWritebackInvalidateAll(void){++caches;}
void sceKernelIcacheInvalidateAll(void){++caches;}
SceModule *sceKernelFindModuleByUID(int id){return browser_present && id==browser.modid?&browser:id==bridge.modid?&bridge:id==netconf.modid && present?&netconf:NULL;}
SceModule *sceKernelFindModuleByName(const char *name)
{return browser_present && !strcmp(name,browser.modname)?&browser:!strcmp(name,"ESP WiFi XMB Bridge")?&bridge:!strcmp(name,"sceNetApctl_Library") && api_present?&api:NULL;}
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER next){STMOD_HANDLER old=handler;handler=next;return old;}
unsigned sctrlHENFindFunction(const char *mod,const char *lib,unsigned nid)
{
    assert(!strcmp(mod,"sceNetApctl_Library"));if(!api_present)return 0;
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i)if(entries[i].nid==nid){assert(!strcmp(lib,entries[i].library));return api.text_addr+entries[i].api_offset;}
    for(unsigned g=0;g<3;++g)for(unsigned i=0;i<browser_import_guards[g]->entry_count;++i) {
        const browser_import_entry_t *e=&browser_import_guards[g]->entries[i];
        if(e->nid==nid){assert(!strcmp(lib,e->library));return api.text_addr+e->api_offset;}
    }
    return 0;
}
static int prior(SceModule *m){assert(m==&netconf || m==&api);++prior_calls;return 17;}
static void seed(void)
{
    memset((void *)(uintptr_t)netconf.text_addr,0,netconf.text_size);
    unsigned *gate=(void *)(uintptr_t)(netconf.text_addr+0x89e4);
    gate[0]=0x3c030a04;gate[1]=0x03e00008;gate[2]=0x90621234;
    for(unsigned i=0;i<sizeof(native_libraries)/sizeof(*native_libraries);++i) {
        unsigned *p=(void *)(uintptr_t)(netconf.text_addr+native_libraries[i].offset);
        p[0]=netconf.text_addr+native_libraries[i].name;p[1]=native_libraries[i].version_flags;p[2]=native_libraries[i].counts;
        p[3]=netconf.text_addr+native_libraries[i].nids;p[4]=netconf.text_addr+native_libraries[i].stubs;
        strcpy((char *)(uintptr_t)(netconf.text_addr+native_libraries[i].name),native_libraries[i].library);
    }
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i) {
        unsigned *p=(void *)(uintptr_t)(netconf.text_addr+entries[i].offset);p[0]=jump(api.text_addr+entries[i].api_offset);p[1]=0;
        *(unsigned *)(uintptr_t)(netconf.text_addr+entries[i].nid_offset)=entries[i].nid;
    }
}
int main(void)
{
    void *memory=mmap((void *)0x09000000u,0x300000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
    assert(memory==(void *)0x09000000u);
    bridge=(SceModule){.modid=1,.text_addr=0x09000000,.text_size=0x1000,.nsegment=2,.segmentaddr={0x09000000,0x09001000},.segmentsize={0x1000,0x3000}};
    netconf=(SceModule){.modid=2,.version={1,1},.text_addr=0x09010000,.text_size=157640};strcpy(netconf.modname,"sceVshNetconf_Module");
    api=(SceModule){.version={3,1},.text_addr=0x09080000,.text_size=0x11e9c};
    wifi_native_hooks_t *request=(void *)0x09001000;request->magic=WIFI_NATIVE_HOOK_MAGIC;request->abi=2;
    for(unsigned i=0;i<WIFI_NATIVE_CALLBACKS;++i)request->callbacks[i]=bridge.text_addr+16*i;
    unsigned saved=request->callbacks[0];request->callbacks[0]=0x88000000;
    assert(wifiNativeRegister((unsigned)(uintptr_t)request,sizeof(*request))<0 && !hooks);request->callbacks[0]=saved;
    assert(!wifiNativeRegister((unsigned)(uintptr_t)request,sizeof(*request)));
    seed();unsigned *last=(void *)(uintptr_t)(netconf.text_addr+entries[10].offset);*last^=4;
    unsigned char before[157640];memcpy(before,(void *)(uintptr_t)netconf.text_addr,sizeof(before));
    assert(observe(&netconf)<0 && hooks->status==-14);
    assert(!memcmp(before,(void *)(uintptr_t)netconf.text_addr,sizeof(before)) && !caches);
    seed();handler=prior;assert(!wifi_native_init());assert(handler(&netconf)==17 && prior_calls==1 && caches==2 && hooks->status==1);
    assert(*(unsigned *)(uintptr_t)(netconf.text_addr+0x89e4)==jump(request->callbacks[WIFI_NATIVE_GATE]));
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i)assert(*(unsigned *)(uintptr_t)(netconf.text_addr+entries[i].offset)==jump(request->callbacks[entries[i].callback]));
    assert(wifi_native_stop()==-2 && handler==on_start && hooks);
    assert(handler(&netconf)==17 && caches==2);
    /* Actual loader state absent from the previous test: syscall21/NOP weak
     * imports, no APCTL provider, followed by a later module relink. */
    owner=-1;seed();api_present=0;
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i)*(unsigned *)(uintptr_t)(netconf.text_addr+entries[i].offset)=0x54c;
    assert(!observe(&netconf) && hooks->status==1);
    api_present=1;strcpy(api.modname,"sceNetApctl_Library");
    for(unsigned i=0;i<3;++i)*(unsigned *)(uintptr_t)(netconf.text_addr+entries[i].offset)=jump(api.text_addr+entries[i].api_offset);
    assert(handler(&api)==17 && hooks->status==2);
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i)assert(*(unsigned *)(uintptr_t)(netconf.text_addr+entries[i].offset)==jump(request->callbacks[entries[i].callback]));
    api_present=0;
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i)*(unsigned *)(uintptr_t)(netconf.text_addr+entries[i].offset)=0x54c;
    assert(!wifiNativeRefresh() && hooks->status==2);
    unsigned *metadata=(void *)(uintptr_t)(netconf.text_addr+entries[3].nid_offset);*metadata^=1;
    memcpy(before,(void *)(uintptr_t)netconf.text_addr,sizeof(before));
    assert(wifiNativeRefresh()<0 && hooks->status==-22 && !memcmp(before,(void *)(uintptr_t)netconf.text_addr,sizeof(before)));
    *metadata^=1;
    *(unsigned *)(uintptr_t)(netconf.text_addr+entries[4].offset)=0x08012345;
    memcpy(before,(void *)(uintptr_t)netconf.text_addr,sizeof(before));
    assert(wifiNativeRefresh()<0 && hooks->status==-14 && !memcmp(before,(void *)(uintptr_t)netconf.text_addr,sizeof(before)));
    /* Browser consumers: refuse any foreign stub before making a write;
     * accept loader placeholders, retain live ownership, then rebind on APCTL. */
    for(unsigned g=0;g<3;++g) {
        const browser_import_guard_t *b=browser_import_guards[g];
        browser=(SceModule){.modid=20+(int)g,.version={b->minor,b->major},.text_addr=0x09100000,.text_size=b->size};
        strcpy(browser.modname,b->name);browser_present=1;api_present=0;
        memset((void *)(uintptr_t)browser.text_addr,0,b->size);
        for(unsigned i=0;i<b->library_count;++i) {
            const browser_import_library_t *l=&b->libraries[i];unsigned *p=(void *)(uintptr_t)(browser.text_addr+l->offset);
            p[0]=browser.text_addr+l->name;p[1]=l->flags;p[2]=l->counts;p[3]=browser.text_addr+l->nids;p[4]=browser.text_addr+l->stubs;
            strcpy((char *)(uintptr_t)(browser.text_addr+l->name),l->library);
        }
        for(unsigned i=0;i<b->entry_count;++i) {
            const browser_import_entry_t *e=&b->entries[i];
            *(unsigned *)(uintptr_t)(browser.text_addr+e->stub)=0x54c;
            *(unsigned *)(uintptr_t)(browser.text_addr+e->nid_offset)=e->nid;
        }
        unsigned *first=(void *)(uintptr_t)(browser.text_addr+b->entries[0].stub);
        unsigned *last_stub=(void *)(uintptr_t)(browser.text_addr+b->entries[b->entry_count-1].stub);
        *last_stub=0x08012345;
        assert(patch_browser_imports(&browser)<0 && *first==(b->entry_count==1?0x08012345u:0x54cu) && hooks->browser_owners[g]==-1);
        *last_stub=0x54c;
        assert(!patch_browser_imports(&browser) && hooks->browser_owners[g]==browser.modid);
        assert(*first==jump(hooks->callbacks[b->entries[0].callback]));
        assert(wifi_native_stop()==-2);
        api_present=1;
        *first=jump(api.text_addr+b->entries[0].api_offset);
        assert(!patch_browser_imports(&browser) && *first==jump(hooks->callbacks[b->entries[0].callback]));
        browser_present=0;
    }
    present=0;assert(!wifi_native_stop() && handler==prior && !hooks);
    munmap(memory,0x300000);puts("PASS: native netconf exact guards, callback ownership, atomic refusal, chained hooks and live-unload refusal");
}
