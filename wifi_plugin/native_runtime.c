/* Owned 6.61 netconf import hooks only. No model or physical WLAN spoofing. */
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsdk.h>
#include <systemctrl.h>
#include <string.h>
#include "native_control.h"
#include "native_runtime.h"
static STMOD_HANDLER previous;
static int installed;
static wifi_native_hooks_t *hooks;
static SceUID owner=-1,bridge_owner=-1;
static unsigned owner_text,bridge_text;
static unsigned observed_words[4];
#include "native_import_guards.h"
static const struct {unsigned offset,nid,callback,api_offset,nid_offset;const char *library;} entries[]={
    {0x2225c,0xb3cf6849,WIFI_NATIVE_SCAN,0x69c,0x22ce4,"sceNetApctl_lib"},
    {0x22264,0x0c7ffa5c,WIFI_NATIVE_LIST,0x7dc,0x22ce8,"sceNetApctl_lib"},
    {0x2226c,0x96beb231,WIFI_NATIVE_ENTRY,0x8d0,0x22cec,"sceNetApctl_lib"},
    {0x22274,0x7cfab990,WIFI_NATIVE_ADD,0xbe4,0x22cf0,"sceNetApctl_lib"},
    {0x2227c,0xe11bafab,WIFI_NATIVE_DEL,0xc64,0x22cf4,"sceNetApctl_lib"},
    {0x22254,0x5deac81b,WIFI_NATIVE_STATE,0xb7c,0x22ce0,"sceNetApctl"},
    {0x2228c,0x69745f0a,WIFI_NATIVE_LOGPTR,0x1040,0x22cfc,"sceNetApctl_lib2"},
    {0x22284,0x3895b348,WIFI_NATIVE_LOGSET,0x109c,0x22cf8,"sceNetApctl_lib2"},
    {0x22244,0xcfb957c6,WIFI_NATIVE_CONNECT,0x5dc,0x22cd8,"sceNetApctl"},
    {0x2224c,0x24fe91a1,WIFI_NATIVE_DISCONNECT,0x64c,0x22cdc,"sceNetApctl"},
    {0x2223c,0x2befdf23,WIFI_NATIVE_INFO,0x9e4,0x22cd4,"sceNetApctl"}
};
static int owned(SceModule *m,unsigned p,unsigned n,int text)
{
    if(!m || (p&3) || n>0x03800000u || p<0x08800000u || p>0x0c000000u-n)return 0;
    if(text)return n<=m->text_size && p>=m->text_addr && p-m->text_addr<=m->text_size-n;
    for(unsigned i=0;i<m->nsegment && i<4;++i)
        if(n<=m->segmentsize[i] && p>=m->segmentaddr[i] && p-m->segmentaddr[i]<=m->segmentsize[i]-n)return 1;
    return 0;
}
static unsigned jump(unsigned target){return 0x08000000u|((target>>2)&0x03ffffffu);}
static int patch_netconf(SceModule *m,int refresh)
{
    if(!hooks || !m || strcmp(m->modname,"sceVshNetconf_Module"))return 0;
    hooks->detail=0;
    SceModule *bridge=sceKernelFindModuleByUID(bridge_owner);
    if(!bridge || bridge->text_addr!=bridge_text)return -1;
    int current=owner>=0 && sceKernelFindModuleByUID(owner)!=NULL;
    if(current && (owner!=m->modid || owner_text!=m->text_addr))return -1;
    if(current && !refresh)return 0;
    if(m->version[0]!=1 || m->version[1]!=1 || m->text_size!=157640 || !owned(m,m->text_addr,m->text_size,1))return hooks->status=-11;
    /* Gate getter at89e4 is LUI v1, JR RA, LBU v0, relocated-data(v1).
     * Only this module's unavailable flag getter is redirected to our backend. */
    unsigned *gate=(void *)(m->text_addr+0x89e4);
    if(current) {
        if(gate[0]!=jump(hooks->callbacks[WIFI_NATIVE_GATE]) || gate[1])return hooks->status=-12;
    } else if((gate[0]&0xffff0000u)!=0x3c030000u || gate[1]!=0x03e00008u ||
        (gate[2]&0xffff0000u)!=0x90620000u)return hooks->status=-12;
    for(unsigned i=0;i<WIFI_NATIVE_CALLBACKS;++i)
        if(!owned(bridge,hooks->callbacks[i],4,1))return hooks->status=-13;
    for(unsigned i=0;i<sizeof(native_libraries)/sizeof(*native_libraries);++i) {
        const unsigned *p=(void *)(m->text_addr+native_libraries[i].offset);
        hooks->detail=native_libraries[i].offset;
        if(p[0]!=m->text_addr+native_libraries[i].name || p[1]!=native_libraries[i].version_flags ||
            p[2]!=native_libraries[i].counts || p[3]!=m->text_addr+native_libraries[i].nids ||
            p[4]!=m->text_addr+native_libraries[i].stubs ||
            memcmp((const char *)(m->text_addr+native_libraries[i].name),native_libraries[i].library,strlen(native_libraries[i].library)+1))return hooks->status=-22;
    }
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i) {
        unsigned function=sctrlHENFindFunction("sceNetApctl_Library",entries[i].library,entries[i].nid);
        SceModule *api=sceKernelFindModuleByName("sceNetApctl_Library");
        unsigned *p=(void *)(m->text_addr+entries[i].offset);
        hooks->detail=entries[i].offset;
        observed_words[0]=p[0];observed_words[1]=p[1];observed_words[2]=function;observed_words[3]=api?api->text_addr:0;
        if(*(unsigned *)(m->text_addr+entries[i].nid_offset)!=entries[i].nid)return hooks->status=-22;
        /* A weak import can legitimately be unlinked at netconf startup. Only
         * the loader's exact syscall21/NOP placeholder, our owned hook, or a
         * resolved jump into the expected APCTL ABI is accepted. */
        int ours=current && p[0]==jump(hooks->callbacks[entries[i].callback]) && !p[1];
        int unlinked=p[0]==0x0000054cu && !p[1];
        int linked=api && api->version[0]==3 && api->version[1]==1 && api->text_size==0x11e9c &&
            function==api->text_addr+entries[i].api_offset && owned(api,function,8,1) && p[0]==jump(function) && !p[1];
        if(!ours && !unlinked && !linked)return hooks->status=-14;
        if(api && (api->version[0]!=3 || api->version[1]!=1 || api->text_size!=0x11e9c))return hooks->status=-14;
    }
    /* Complete preflight precedes every write. All destinations belong to the
     * registered user bridge; netconf and bridge share the user jump region. */
    gate[0]=jump(hooks->callbacks[WIFI_NATIVE_GATE]);gate[1]=0;
    for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);++i) {
        unsigned *p=(void *)(m->text_addr+entries[i].offset);
        p[0]=jump(hooks->callbacks[entries[i].callback]);p[1]=0;
    }
    sceKernelDcacheWritebackInvalidateAll();sceKernelIcacheInvalidateAll();
    owner=m->modid;owner_text=m->text_addr;hooks->owner=owner;hooks->detail=0;hooks->status=refresh?2:1;return 0;
}
static int observe(SceModule *m)
{
    if(m && !strcmp(m->modname,"sceNetApctl_Library") && owner>=0)
        return patch_netconf(sceKernelFindModuleByUID(owner),1);
    return patch_netconf(m,0);
}
static int on_start(SceModule *m)
{int r=previous?previous(m):0;int intr=sceKernelCpuSuspendIntr();observe(m);sceKernelCpuResumeIntr(intr);return r;}
int wifiNativeRefresh(void)
{
    unsigned k1=pspSdkSetK1(0);int intr=sceKernelCpuSuspendIntr();
    int result=owner>=0?patch_netconf(sceKernelFindModuleByUID(owner),1):0;
    sceKernelCpuResumeIntr(intr);pspSdkSetK1(k1);return result;
}
int wifiNativeRegister(unsigned address,unsigned size)
{
    unsigned k1=pspSdkSetK1(0);int result=-1;
    SceModule *m=sceKernelFindModuleByName("ESP WiFi XMB Bridge");
    if(!hooks && size==sizeof(wifi_native_hooks_t) && owned(m,address,size,0)) {
        wifi_native_hooks_t *request=(void *)address;
        if(request->magic==WIFI_NATIVE_HOOK_MAGIC && request->abi==1) {
            unsigned i;for(i=0;i<WIFI_NATIVE_CALLBACKS;++i)if(!owned(m,request->callbacks[i],4,1))break;
            if(i==WIFI_NATIVE_CALLBACKS){hooks=request;bridge_owner=m->modid;bridge_text=m->text_addr;hooks->status=0;hooks->owner=-1;result=0;}
        }
    }
    pspSdkSetK1(k1);return result;
}
int wifi_native_init(void)
{if(installed)return -1;previous=sctrlHENSetStartModuleHandler(on_start);installed=1;return 0;}
int wifi_native_stop(void)
{
    if(owner>=0 && sceKernelFindModuleByUID(owner))return -2;
    if(installed){STMOD_HANDLER current=sctrlHENSetStartModuleHandler(previous);if(current!=on_start){sctrlHENSetStartModuleHandler(current);return -2;}installed=0;}
    hooks=NULL;return 0;
}

int wifi_native_status(void){return hooks?hooks->status:-20;}

unsigned wifi_native_detail(void){return hooks?hooks->detail:0;}

void wifi_native_observed(unsigned out[4]){memcpy(out,observed_words,sizeof(observed_words));}
