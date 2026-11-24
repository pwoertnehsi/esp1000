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
static unsigned browser_owner_text[WIFI_NATIVE_CONSUMERS];
/* Debug-only radio tracing: all guards precede the normal import writes too. */
static int radio_trace_preflight(SceModule *m,int current,unsigned *send)
{
    const unsigned *p=(const void *)(m->text_addr+0x1e41c);
    if(p[0]!=m->text_addr+0x1e5bc || p[1]!=0x90011 || p[2]!=0x170005 ||
        p[3]!=m->text_addr+0x1e720 || p[4]!=m->text_addr+0x1dc98 ||
        strcmp((const char *)(m->text_addr+0x1e5bc),"sceHttp") ||
        *(unsigned *)(m->text_addr+0x1e75c)!=0xbb70706fu)return -36;
    SceModule *http=sceKernelFindModuleByName("SceHttp_Library");
    *send=sctrlHENFindFunction("SceHttp_Library","sceHttp",0xbb70706fu);
    if(!http || http->version[0]!=1 || http->version[1]!=2 || http->text_size!=0x18d1c ||
        *send!=http->text_addr+0x1e28 || !owned(http,*send,8,1))return -37;
    p=(const void *)(m->text_addr+0x1dd10);
    int ours=current && p[0]==jump(hooks->callbacks[WIFI_NATIVE_RADIO_SEND]) && !p[1];
    if(!ours && !((p[0]==jump(*send) || p[0]==0x54c) && !p[1]))return -38;
    p=(const void *)(m->text_addr+0xff9c);
    unsigned call=0x0c000000u|((hooks->callbacks[WIFI_NATIVE_RADIO_REQUEST]>>2)&0x03ffffffu);
    if(p[0]!=0x8c820844u || p[1]!=0x10400005u || p[2]!=0x00808021u ||
        (p[3]!=0x0040f809u && !(current && p[3]==call)) || p[4]!=0x8c84084cu)return -39;
    return 0;
}
static int patch_browser_imports(SceModule *m)
{
    if(!hooks || !m)return 0;
    unsigned index;
    for(index=0;index<WIFI_NATIVE_CONSUMERS;++index)if(!strcmp(m->modname,browser_import_guards[index]->name))break;
    if(index==WIFI_NATIVE_CONSUMERS)return 0;
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
    unsigned radio_send=0;
    if(ESP1000_DIAGNOSTICS && index==WIFI_NATIVE_RADIO) {
        int r=radio_trace_preflight(m,current,&radio_send);
        if(r<0)return hooks->browser_status=r;
        if(!owned(bridge,hooks->callbacks[WIFI_NATIVE_RADIO_REQUEST],4,1) ||
            !owned(bridge,hooks->callbacks[WIFI_NATIVE_RADIO_SEND],4,1))return hooks->browser_status=-35;
    }
    for(unsigned i=0;i<g->entry_count;++i) {
        const browser_import_entry_t *e=&g->entries[i];unsigned *p=(void *)(m->text_addr+e->stub);
        p[0]=jump(hooks->callbacks[e->callback]);p[1]=0;
    }
    if(radio_send) {
        hooks->radio_send_original=radio_send;
        unsigned *p=(void *)(m->text_addr+0x1dd10);
        p[0]=jump(hooks->callbacks[WIFI_NATIVE_RADIO_SEND]);p[1]=0;
        p=(void *)(m->text_addr+0xffa8);
        *p=0x0c000000u|((hooks->callbacks[WIFI_NATIVE_RADIO_REQUEST]>>2)&0x03ffffffu);
    }
    sceKernelDcacheWritebackInvalidateAll();sceKernelIcacheInvalidateAll();
    browser_owner_text[index]=m->text_addr;hooks->browser_owners[index]=m->modid;
    hooks->consumer_mask|=1u<<index;hooks->browser_status=1;return 0;
}
