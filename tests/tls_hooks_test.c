#define _GNU_SOURCE
#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
typedef struct {unsigned text_addr,text_size;unsigned char version[2];int modid;} SceModule;
static SceModule http={0x09000000,0x18d1c,{1,2},1},ssl={0x09400000,0x2ebc0,{1,2},2},net={0x09800000,0x31d70,{3,1},3},bridge={0x09c00000,0x10000,{0,18},4};
static int provider=1,caches;
static SceModule *sceKernelFindModuleByName(const char *name)
{if(!strcmp(name,"ESP WiFi XMB Bridge"))return &bridge;if(!strcmp(name,"SceHttp_Library"))return &http;if(!strcmp(name,"sceSsl_Module"))return provider?&ssl:NULL;if(!strcmp(name,"sceNetInet_Library"))return &net;return NULL;}
static SceModule *sceKernelFindModuleByUID(int id)
{return id==1?&http:id==2?&ssl:id==3?&net:id==4?&bridge:NULL;}
static int owned(SceModule *m,unsigned p,unsigned n,int text)
{(void)text;return m && !(p&3) && p>=m->text_addr && n<=m->text_size && p-m->text_addr<=m->text_size-n;}
static unsigned jump(unsigned target){return 0x08000000|((target>>2)&0x03ffffff);}
static unsigned sctrlHENFindFunction(const char *,const char *,unsigned);
static unsigned pspSdkSetK1(unsigned k){return k;}
static int sceKernelCpuSuspendIntr(void){return 1;}
static void sceKernelCpuResumeIntr(int n){assert(n==1);}
static void sceKernelDcacheWritebackInvalidateAll(void){++caches;}
static void sceKernelIcacheInvalidateAll(void){++caches;}
#include "../wifi_plugin/tls_hooks.h"
static unsigned sctrlHENFindFunction(const char *m,const char *l,unsigned nid)
{
    (void)m;(void)l;
    for(unsigned i=0;i<sizeof(tls_entries)/sizeof(*tls_entries);++i)if(tls_entries[i].nid==nid)return tls_entries[i].slot==WT_CLOSE?net.text_addr+0x1000:ssl.text_addr+tls_entries[i].api;
    return 0;
}
static void seed(void)
{
    memset((void *)(uintptr_t)http.text_addr,0,http.text_size);
    unsigned *p=(void *)(uintptr_t)(http.text_addr+0x17d84);
    p[0]=http.text_addr+0x17ecc;p[1]=0x90011;p[2]=0x360005;p[3]=http.text_addr+0x183b4;p[4]=http.text_addr+0x17b38;
    p=(void *)(uintptr_t)(http.text_addr+0x17d70);p[0]=http.text_addr+0x17ebc;p[1]=0x10011;p[2]=0xe0005;p[3]=http.text_addr+0x18310;p[4]=http.text_addr+0x179f0;
    strcpy((void *)(uintptr_t)(http.text_addr+0x17ecc),"sceSsl_lib");strcpy((void *)(uintptr_t)(http.text_addr+0x17ebc),"sceNetInet");
    p=(void *)(uintptr_t)(http.text_addr+0x107fc);p[0]=0x0c000000|(((http.text_addr+0x11098)>>2)&0x03ffffff);p[1]=0x02a02021;
    *(unsigned *)(uintptr_t)(http.text_addr+0x10744)=0x8c9003a4;*(unsigned *)(uintptr_t)(http.text_addr+0x10750)=0x8c9003a0;*(unsigned *)(uintptr_t)(http.text_addr+0x105e0)=0x8e350038;
    for(unsigned i=0;i<sizeof(tls_entries)/sizeof(*tls_entries);++i) {
        unsigned slot=tls_entries[i].slot;
        unsigned nid_base=slot==WT_CLOSE?0x18310:0x183b4,stub_base=slot==WT_CLOSE?0x179f0:0x17b38;
        *(unsigned *)(uintptr_t)(http.text_addr+nid_base+(tls_entries[i].stub-stub_base)/2)=tls_entries[i].nid;
        p=(void *)(uintptr_t)(http.text_addr+tls_entries[i].stub);p[0]=jump(sctrlHENFindFunction(NULL,NULL,tls_entries[i].nid));p[1]=0;
    }
    tls_owner=-1;tls_http_text=0;
}
int main(void)
{
    assert(mmap((void *)(uintptr_t)http.text_addr,0x20000,3,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void *)-1);
    assert(mmap((void *)(uintptr_t)bridge.text_addr,0x10000,3,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void *)-1);
    wifi_tls_control_t *ctl=(void *)(uintptr_t)(bridge.text_addr+0x8000);ctl->magic=WIFI_TLS_MAGIC;ctl->abi=WIFI_TLS_ABI;
    for(unsigned i=0;i<WIFI_TLS_COUNT;++i)ctl->callbacks[i]=bridge.text_addr+0x100+i*4;
    seed();provider=0;assert(!wifiTlsRegister((unsigned)(uintptr_t)ctl,sizeof(*ctl)) && !caches && ctl->status==0);provider=1;
    unsigned original=*(unsigned *)(uintptr_t)(http.text_addr+0x107fc);
    http.text_size--;assert(patch_tls()<0 && !caches && *(unsigned *)(uintptr_t)(http.text_addr+0x107fc)==original);http.text_size++;
    *(unsigned *)(uintptr_t)(http.text_addr+0x183cc)^=1;assert(patch_tls()<0 && !caches);*(unsigned *)(uintptr_t)(http.text_addr+0x183cc)^=1;
    unsigned old=ctl->callbacks[3];ctl->callbacks[3]=0x88000100;assert(patch_tls()<0 && !caches);ctl->callbacks[3]=old;
    assert(!patch_tls() && caches==2 && ctl->status==1 && !tls_can_stop());
    assert(!patch_tls() && caches==4);
    unsigned *stub=(void *)(uintptr_t)(http.text_addr+0x17cd0);stub[0]^=1;assert(patch_tls()<0 && caches==4);
    puts("PASS: actual TLS guard atomic preflight, owned callbacks, provider wait, NID verification and foreign-stub refusal");
}
