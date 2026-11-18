/* Included after native_runtime's ownership helpers. Browser consumers keep
 * their own imports; the real APCTL provider and unrelated clients are untouched. */
typedef struct {unsigned offset,name,flags,counts,nids,stubs;const char *library;} browser_import_library_t;
typedef struct {unsigned stub,nid_offset,nid,api_offset,callback;const char *library;} browser_import_entry_t;
typedef struct {
    const char *name;unsigned minor,major,size;
    const browser_import_library_t *libraries;unsigned library_count;
    const browser_import_entry_t *entries;unsigned entry_count;
} browser_import_guard_t;
#include "browser_import_guards.h"
static unsigned browser_owner_text[3];
static int patch_browser_imports(SceModule *m)
{
    if(!hooks || !m)return 0;
    unsigned index;
    for(index=0;index<3;++index)if(!strcmp(m->modname,browser_import_guards[index]->name))break;
    if(index==3)return 0;
    const browser_import_guard_t *g=browser_import_guards[index];
    SceModule *bridge=sceKernelFindModuleByUID(bridge_owner);
    if(!bridge || bridge->text_addr!=bridge_text)return hooks->browser_status=-30;
    int current=hooks->browser_owners[index]>=0 && sceKernelFindModuleByUID(hooks->browser_owners[index]);
    if(current && (hooks->browser_owners[index]!=m->modid || browser_owner_text[index]!=m->text_addr))return hooks->browser_status=-31;
    if(m->version[0]!=g->minor || m->version[1]!=g->major || m->text_size!=g->size || !owned(m,m->text_addr,m->text_size,1))return hooks->browser_status=-32;
    for(unsigned i=0;i<g->library_count;++i) {
        const browser_import_library_t *l=&g->libraries[i];
        const unsigned *p=(void *)(m->text_addr+l->offset);
        if(p[0]!=m->text_addr+l->name || p[1]!=l->flags || p[2]!=l->counts || p[3]!=m->text_addr+l->nids || p[4]!=m->text_addr+l->stubs ||
            memcmp((const char *)(m->text_addr+l->name),l->library,strlen(l->library)+1))return hooks->browser_status=-33;
    }
    SceModule *api=sceKernelFindModuleByName("sceNetApctl_Library");
    if(api && (api->version[0]!=3 || api->version[1]!=1 || api->text_size!=0x11e9c))return hooks->browser_status=-34;
    for(unsigned i=0;i<g->entry_count;++i) {
        const browser_import_entry_t *e=&g->entries[i];
        unsigned target=hooks->callbacks[e->callback],*p=(void *)(m->text_addr+e->stub);
        unsigned function=sctrlHENFindFunction("sceNetApctl_Library",e->library,e->nid);
        int ours=current && p[0]==jump(target) && !p[1];
        int unlinked=p[0]==0x0000054cu && !p[1];
        int linked=api && function==api->text_addr+e->api_offset && owned(api,function,8,1) && p[0]==jump(function) && !p[1];
        if(!owned(bridge,target,4,1) || *(unsigned *)(m->text_addr+e->nid_offset)!=e->nid || (!ours && !unlinked && !linked))return hooks->browser_status=-35;
    }
    for(unsigned i=0;i<g->entry_count;++i) {
        const browser_import_entry_t *e=&g->entries[i];unsigned *p=(void *)(m->text_addr+e->stub);
        p[0]=jump(hooks->callbacks[e->callback]);p[1]=0;
    }
    sceKernelDcacheWritebackInvalidateAll();sceKernelIcacheInvalidateAll();
    browser_owner_text[index]=m->text_addr;hooks->browser_owners[index]=m->modid;
    hooks->browser_status=1;return 0;
}
