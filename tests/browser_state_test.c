#define _GNU_SOURCE
#include "browser_state_shim.h"
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
static int call_close(void *address);
#define BROWSER_CALL_CLOSE(address) call_close(address)
#include "../browser_local/app/browser_state.c"
static SceModule module;
static int close_calls,reports,available=1;
int kuKernelFindModuleByName(const char *name,SceModule *m)
{assert(!strcmp(name,"sceVshHVUtility_Module"));*m=module;return available?0:-1;}
int sceKernelReferThreadStatus(int id,SceKernelThreadInfo *info)
{assert(id==123);strcpy(info->name,"Browser");info->status=4;info->waitType=3;info->waitId=456;return 0;}
static int call_close(void *address)
{assert(address==(void *)(uintptr_t)(module.text_addr+0x2ae8));++close_calls;*(unsigned *)(uintptr_t)(module.segmentaddr[1]+0xd0cc)=7;return 0;}
static void report(const char *format,...){assert(format);++reports;}
int main(void)
{
    void *text=mmap((void *)0x0a100000,0x10000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    void *data=mmap((void *)0x0a110000,0x12000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(text==(void *)0x0a100000 && data==(void *)0x0a110000);
    module=(SceModule){.version={1,1},.nsegment=2,.text_addr=0x0a100000,.text_size=0xe808,
        .segmentaddr={0x0a100000,0x0a110000},.segmentsize={0xe808,0x12000},.modid=1};
    const unsigned setters[3]={0x2850,0x2868,0x2880},targets[3]={0xd0cc,0xd0d0,0x14c};
    for(unsigned i=0;i<3;++i) {
        unsigned *code=(void *)((char *)text+setters[i]);unsigned address=module.segmentaddr[1]+targets[i];
        code[0]=0x3c020000|((address+0x8000)>>16);code[1]=0x03e00008;code[2]=0xac440000|(address&65535);
    }
    unsigned *entry=(void *)((char *)text+0x2ae8);entry[0]=0x27bdfff0;entry[1]=0xafb10004;entry[3]=0x24020004;
    *(unsigned *)((char *)data+0xd0cc)=2;*(unsigned *)((char *)data+0x28c)=123;
    browser_log_state(report);assert(reports==1);browser_log_state(report);assert(reports==1);
    unsigned *setter=(void *)((char *)text+0x2850);unsigned good=setter[0];
    setter[0]=0x3c020a10;assert(browser_request_close()<0 && !close_calls);setter[0]=good;
    module.version[0]=2;assert(browser_request_close()<0 && !close_calls);module.version[0]=1;
    entry[1]^=1;assert(browser_request_close()<0 && !close_calls);entry[1]^=1;
    assert(!browser_request_close() && close_calls==1);assert(!browser_request_close() && close_calls==1);
    browser_log_state(report);assert(reports==3);
    available=0;assert(browser_request_close()<0);
}
