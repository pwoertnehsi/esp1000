/* Included in native_runtime after owned()/jump(). VSH owned HTTP imports only. */
#include "tls_control.h"
#include "tls_guards.h"
static wifi_tls_control_t *tls_hooks;
static int tls_bridge=-1,tls_owner=-1;
static unsigned tls_bridge_text,tls_http_text;
static int patch_tls(void)
{
    if(!tls_hooks)return 0;
    SceModule *http=sceKernelFindModuleByName("SceHttp_Library");
    SceModule *ssl=sceKernelFindModuleByName("sceSsl_Module");
    SceModule *net=sceKernelFindModuleByName("sceNetInet_Library");
    SceModule *bridge=sceKernelFindModuleByUID(tls_bridge);
    if(!http || !ssl || !net)return 0;
    if(!bridge || bridge->text_addr!=tls_bridge_text)return tls_hooks->status=-1;
    if(http->text_size!=WIFI_TLS_HTTP_SIZE || http->version[0]!=1 || http->version[1]!=2 ||
       ssl->text_size!=WIFI_TLS_SSL_SIZE || ssl->version[0]!=1 || ssl->version[1]!=2 ||
       net->text_size!=0x31d70 || net->version[0]!=3 || net->version[1]!=1)return tls_hooks->status=-2;
    if(tls_owner>=0 && sceKernelFindModuleByUID(tls_owner) && (tls_owner!=http->modid || tls_http_text!=http->text_addr))return tls_hooks->status=-3;
    int ours=tls_owner==http->modid && tls_http_text==http->text_addr;
    const unsigned *lib=(void *)(http->text_addr+0x17d84);
    if(lib[0]!=http->text_addr+0x17ecc || lib[1]!=0x90011 || lib[2]!=0x360005 || lib[3]!=http->text_addr+0x183b4 || lib[4]!=http->text_addr+0x17b38)return tls_hooks->status=-4;
    const unsigned *inet=(void *)(http->text_addr+0x17d70);
    if(inet[0]!=http->text_addr+0x17ebc || inet[1]!=0x10011 || inet[2]!=0xe0005 || inet[3]!=http->text_addr+0x18310 || inet[4]!=http->text_addr+0x179f0)return tls_hooks->status=-4;
    if(strcmp((char *)(http->text_addr+0x17ecc),"sceSsl_lib") || strcmp((char *)(http->text_addr+0x17ebc),"sceNetInet"))return tls_hooks->status=-4;
    unsigned *call=(void *)(http->text_addr+0x107fc);
    unsigned original_call=0x0c000000|(((http->text_addr+0x11098)>>2)&0x03ffffff);
    unsigned new_call=0x0c000000|((tls_hooks->callbacks[WT_CONNECT]>>2)&0x03ffffff);
    if(call[0]!=(ours?new_call:original_call) || call[1]!=0x02a02021)return tls_hooks->status=-5;
    if(*(unsigned *)(http->text_addr+0x10744)!=0x8c9003a4 || *(unsigned *)(http->text_addr+0x10750)!=0x8c9003a0 || *(unsigned *)(http->text_addr+0x105e0)!=0x8e350038)return tls_hooks->status=-5;
    unsigned functions[WIFI_TLS_COUNT];functions[WT_CONNECT]=http->text_addr+0x11098;
    for(unsigned i=0;i<WIFI_TLS_COUNT;++i)if(!owned(bridge,tls_hooks->callbacks[i],4,1))return tls_hooks->status=-6;
    for(unsigned i=0;i<sizeof(tls_entries)/sizeof(*tls_entries);++i) {
        unsigned slot=tls_entries[i].slot,nid=tls_entries[i].nid;
        unsigned nid_base=slot==WT_CLOSE?0x18310:0x183b4,stub_base=slot==WT_CLOSE?0x179f0:0x17b38;
        if(*(unsigned *)(http->text_addr+nid_base+(tls_entries[i].stub-stub_base)/2)!=nid)return tls_hooks->status=-10;
        unsigned function=sctrlHENFindFunction(slot==WT_CLOSE?"sceNetInet_Library":"sceSsl_Module",slot==WT_CLOSE?"sceNetInet":"sceSsl_lib",nid);
        if(!function || (slot!=WT_CLOSE && function!=ssl->text_addr+tls_entries[i].api) || !owned(slot==WT_CLOSE?net:ssl,function,4,1))return tls_hooks->status=-7;
        unsigned *stub=(void *)(http->text_addr+tls_entries[i].stub);
        unsigned nid_at=slot==WT_CLOSE?0x18310+(tls_entries[i].stub-0x179f0)/2:0x183b4+(tls_entries[i].stub-0x17b38)/2;
        if(*(unsigned *)(http->text_addr+nid_at)!=nid)return tls_hooks->status=-8;
        int linked=stub[0]==jump(function) && !stub[1];
        int unlinked=slot!=WT_CLOSE && stub[0]==0x0000054c && !stub[1];
        int patched=ours && stub[0]==jump(tls_hooks->callbacks[slot]) && !stub[1];
        if(!linked && !unlinked && !patched)return tls_hooks->status=-9;
        functions[slot]=function;
    }
    memcpy(tls_hooks->originals,functions,sizeof(functions));__sync_synchronize();
    call[0]=new_call;
    for(unsigned i=0;i<sizeof(tls_entries)/sizeof(*tls_entries);++i) {
        unsigned *stub=(void *)(http->text_addr+tls_entries[i].stub);
        stub[0]=jump(tls_hooks->callbacks[tls_entries[i].slot]);stub[1]=0;
    }
    sceKernelDcacheWritebackInvalidateAll();sceKernelIcacheInvalidateAll();
    tls_owner=http->modid;tls_http_text=http->text_addr;tls_hooks->owner=tls_owner;tls_hooks->status=1;return 0;
}
int wifiTlsRegister(unsigned address,unsigned size)
{
    unsigned k1=pspSdkSetK1(0);int result=-1;
    SceModule *m=sceKernelFindModuleByName("ESP WiFi XMB Bridge");
    if(!tls_hooks && size==sizeof(wifi_tls_control_t) && owned(m,address,size,0)) {
        wifi_tls_control_t *request=(void *)address;
        if(request->magic==WIFI_TLS_MAGIC && request->abi==WIFI_TLS_ABI) {
            unsigned i;for(i=0;i<WIFI_TLS_COUNT;++i)if(!owned(m,request->callbacks[i],4,1))break;
            if(i==WIFI_TLS_COUNT){tls_hooks=request;tls_bridge=m->modid;tls_bridge_text=m->text_addr;request->owner=-1;request->status=0;int intr=sceKernelCpuSuspendIntr();patch_tls();sceKernelCpuResumeIntr(intr);result=0;}
        }
    }
    pspSdkSetK1(k1);return result;
}
static int tls_can_stop(void)
{return !tls_hooks || (!tls_hooks->active && (tls_owner<0 || !sceKernelFindModuleByUID(tls_owner)));}

int wifi_native_tls_status(void){return tls_hooks?tls_hooks->status:-20;}
