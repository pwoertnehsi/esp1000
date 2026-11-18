#include <pspkernel.h>
#include <psploadcore.h>
#include <kubridge.h>
#include <string.h>
#include "browser_state.h"
#ifndef BROWSER_CALL_CLOSE
#define BROWSER_CALL_CLOSE(address) (((int (*)(int))(address))(1))
#endif
static int wrapper(SceModule *m)
{
    return kuKernelFindModuleByName("sceVshHVUtility_Module",m)>=0
        && m->version[0]==1 && m->version[1]==1 && m->text_size==0xe808
        && m->nsegment==2 && m->text_addr==m->segmentaddr[0]
        && m->segmentaddr[0]>=0x08800000 && m->segmentaddr[0]+m->segmentsize[0]<=0x0c000000
        && m->segmentaddr[1]>=0x08800000 && m->segmentsize[1]>=0x2a0
        && m->segmentaddr[1]+m->segmentsize[1]<=0x0c000000;
}
/* Sony relocations select their target segment, so a raw instruction's low
 * offset is not necessarily relative to text_addr. Decode the relocated
 * setter's address and require ownership by this module's writable segment. */
static volatile unsigned *state_address(const SceModule *m,unsigned setter)
{
    const unsigned *code=(void *)(m->text_addr+setter);
    if((code[0]&0xffff0000u)!=0x3c020000u || code[1]!=0x03e00008u
       || (code[2]&0xffff0000u)!=0xac440000u)return NULL;
    unsigned address=((code[0]&65535u)<<16)+(int)(short)(code[2]&65535u);
    if((address&3) || address<m->segmentaddr[1] || m->segmentsize[1]<4
       || address-m->segmentaddr[1]>m->segmentsize[1]-4)return NULL;
    return (void *)address;
}
int browser_request_close(void)
{
    SceModule m;if(!wrapper(&m))return -1;
    volatile unsigned *state_ptr=state_address(&m,0x2850);if(!state_ptr)return -1;
    unsigned state=*state_ptr;
    if(state>=7)return 0; /* A normal close is already being processed. */
    /* Same callback Sony installs at viewer context+0x40 for menu close.
     * No forced status change, module unload or memory free. */
    unsigned *entry=(void *)(m.text_addr+0x2ae8);
    if(entry[0]!=0x27bdfff0 || entry[1]!=0xafb10004 || entry[3]!=0x24020004)return -1;
    return BROWSER_CALL_CLOSE(entry);
}
void browser_log_state(void (*report)(const char *,...))
{
    static SceUID old_uid=-1;
    static unsigned old_state=~0u,old_dialog=~0u,old_mode=~0u;
    SceModule m;if(!wrapper(&m))return;
    volatile unsigned *state_ptr=state_address(&m,0x2850),*dialog_ptr=state_address(&m,0x2868),*mode_ptr=state_address(&m,0x2880);
    if(!state_ptr || !dialog_ptr || !mode_ptr)return;
    unsigned state=*state_ptr,dialog=*dialog_ptr,mode=*mode_ptr;
    if(old_uid==m.modid && state==old_state && dialog==old_dialog && mode==old_mode)return;
    old_uid=m.modid;old_state=state;old_dialog=dialog;old_mode=mode;
    report("BROWSER_CORE state=%u dialog=%u mode=%u\n",state,dialog,mode);
    if(state>=7) {
        int id=*(volatile int *)(m.segmentaddr[1]+0x28c);
        for(unsigned depth=0;id>0 && depth<4;++depth) {
            SceKernelThreadInfo info;memset(&info,0,sizeof(info));info.size=sizeof(info);
            int r=sceKernelReferThreadStatus(id,&info);
            report("BROWSER_WAIT depth=%u uid=%08x result=%08x name=%s status=%08x type=%u object=%08x\n",
                   depth,id,r,r?"?":info.name,r?0:info.status,r?0:info.waitType,r?0:info.waitId);
            if(r || info.waitType!=9 || info.waitId==id)break;
            id=info.waitId;
        }
    }
}
