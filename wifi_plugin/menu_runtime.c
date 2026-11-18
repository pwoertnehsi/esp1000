/* Scoped 6.61 VSH data/call-site patch. No flash or global model changes. */
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsdk.h>
#include <systemctrl.h>
#include <string.h>
#include "menu_control.h"
#include "menu_patch.h"
#include "menu_runtime.h"
static STMOD_HANDLER previous;
static int installed,status;
static volatile unsigned in_handler;
static SceUID owner=-1;
static unsigned owner_text;
static uint32_t callbacks[2];
static uint32_t *trampoline;
static unsigned activity_calls;
static wifi_menu_control_t *control;
static wifi_menu_snapshot_t evidence;
void wifi_menu_snapshot(wifi_menu_snapshot_t *out)
{
    int intr=sceKernelCpuSuspendIntr();*out=evidence;sceKernelCpuResumeIntr(intr);
}
static int within(unsigned address,unsigned length,unsigned base,unsigned size)
{return length<=size && address>=base && address-base<=size-length;}
static int user_text(SceModule *m,unsigned address)
{
    /* E1000 VSH was observed at 0x0a040f00: do not use the 32 MB
     * physical RAM ceiling as a user address limit on this 64 MB model.
     * Require the entire registered text span to fit, not just its start. */
    return m && within(m->text_addr,m->text_size,0x08800000u,0x03800000u) &&
        !(address&3) && within(address,4,m->text_addr,m->text_size);
}
static int bridge_data(SceModule *m,unsigned address,unsigned size)
{
    if(address&3)return 0;
    for(unsigned i=0;i<m->nsegment && i<4;++i)
        if(within(m->segmentaddr[i],m->segmentsize[i],0x08800000u,0x03800000u) &&
            within(address,size,m->segmentaddr[i],m->segmentsize[i]))return 1;
    return 0;
}
static int record_result(SceModule *m,int result)
{
    unsigned serial=evidence.serial+1;
    memset(&evidence,0,sizeof(evidence));
    evidence.version[0]=m->version[0];evidence.version[1]=m->version[1];
    evidence.attribute=m->attribute;evidence.text=m->text_addr;evidence.size=m->text_size;
    evidence.segments=m->nsegment;evidence.result=result;evidence.user_address=user_text(m,m->text_addr);
    evidence.bridge_callbacks[0]=callbacks[0];evidence.bridge_callbacks[1]=callbacks[1];
    for(unsigned i=0;i<m->nsegment && i<4;++i) {
        evidence.segment_address[i]=m->segmentaddr[i];evidence.segment_size[i]=m->segmentsize[i];
    }
    /* Read only registered module text, even when the narrower callback guard
     * rejects its address. Never dereference an unchecked diagnostic pointer. */
    int mapped=0;
    for(unsigned i=0;i<m->nsegment && i<4;++i)
        if(within(m->text_addr,m->text_size,m->segmentaddr[i],m->segmentsize[i]))mapped=1;
    if(mapped && !(m->text_addr&3) && m->text_size<=0x100000u) {
        for(unsigned i=0;i<MENU_LENGTH(menu_anchors) && i<16;++i) {
            unsigned offset=menu_anchors[i].offset;
            if(m->text_size<4 || offset>m->text_size-4)break;
            evidence.anchor_words[i]=*(const unsigned *)(m->text_addr+offset);
            evidence.anchor_count=i+1;
        }
        for(unsigned i=0;i<MENU_LENGTH(menu_rows) && i<16;++i) {
            unsigned offset=menu_rows[i].offset+8;
            if(m->text_size<4 || offset>m->text_size-4)break;
            evidence.row_masks[i]=*(const unsigned *)(m->text_addr+offset);
            evidence.row_count=i+1;
        }
    }
    if(owner>=0 && m->nsegment==2 && m->text_addr==owner_text &&
        within(m->segmentaddr[1],m->segmentsize[1],0x08800000u,0x03800000u) &&
        m->segmentsize[1]>=0x72e8u) {
        const unsigned char *data=(void *)m->segmentaddr[1];
        for(unsigned i=0;i<8;++i) {
            evidence.root_resources[i]=*(const unsigned *)(data+0x61b8+88*i);
            evidence.network_resources[i]=*(const unsigned *)(data+0x7028+88*i);
        }
        unsigned instance=*(const unsigned *)(data+0x618c);
        if(!(instance&3) && within(instance,0x1b50,0x08800000u,0x03800000u)) {
            evidence.menu_instance=instance;
            for(unsigned i=0;i<8;++i)evidence.item_counts[i]=*(const unsigned *)(instance+0xe8c+12*i);
            unsigned parent=*(const unsigned *)(instance+0xa6c);
            if(!(parent&3) && within(parent,0x364,0x08800000u,0x03800000u)) {
                evidence.menu_parent=parent;evidence.category_count=*(const unsigned *)(parent+0x334);
            }
        }
    }
    evidence.serial=serial;return status=result;
}
static int observe(SceModule *m)
{
    if(!control || !m || strcmp(m->modname,"vsh_module"))return 0;
    if(owner>=0)return owner==m->modid && owner_text==m->text_addr?0:-10;
    if(m->version[0]!=1 || m->version[1]!=1 || !user_text(m,m->text_addr))return record_result(m,-11);
    if(m->nsegment!=2 || m->segmentaddr[1]!=m->text_addr+WIFI_VSH_TEXT_SIZE ||
        !within(m->segmentaddr[1],m->segmentsize[1],0x08800000u,0x03800000u))return record_result(m,-13);
    unsigned char *data=(void *)m->segmentaddr[1];
    int r=menu_patch_prepare((void *)m->text_addr,m->text_size,m->text_addr,callbacks,
        data,m->segmentsize[1],trampoline,(unsigned)(uintptr_t)trampoline);
    if(r<0)return record_result(m,r);
    /* Preflight includes the native entry and our bridge-owned trampoline.
     * Leave all CFW-owned call sites alone, including hooks installed later. */
    menu_patch_apply((void *)m->text_addr,data,callbacks,trampoline,m->text_addr);
    sceKernelDcacheWritebackInvalidateAll();sceKernelIcacheInvalidateAll();
    control->active=1;owner=m->modid;owner_text=m->text_addr;
    return record_result(m,1);
}
static int on_start(SceModule *m)
{
    int intr=sceKernelCpuSuspendIntr();++in_handler;sceKernelCpuResumeIntr(intr);
    int r=previous?previous(m):0;
    intr=sceKernelCpuSuspendIntr();observe(m);--in_handler;sceKernelCpuResumeIntr(intr);
    return r;
}
int wifiMenuRegister(unsigned address,unsigned size,unsigned first,unsigned second)
{
    unsigned k1=pspSdkSetK1(0);int result=-20;
    status=-20;
    int intr=sceKernelCpuSuspendIntr();
    SceModule *bridge=sceKernelFindModuleByName("ESP WiFi XMB Bridge");
    if(installed && !control && bridge && size==sizeof(wifi_menu_control_t) &&
        bridge_data(bridge,address,size) && user_text(bridge,first) && user_text(bridge,second)) {
        wifi_menu_control_t *request=(void *)address;
        if(request->magic==WIFI_MENU_MAGIC && request->abi==WIFI_MENU_ABI &&
            !request->active && !request->pending && request->original[0]==request->original[1] &&
            user_text(bridge,request->original[0]) &&
            within(request->original[0],16,bridge->text_addr,bridge->text_size)) {
            control=request;callbacks[0]=first;callbacks[1]=second;
            trampoline=(void *)request->original[0];
            result=observe(sceKernelFindModuleByName("vsh_module"));
            if(result<0){control=NULL;trampoline=NULL;callbacks[0]=callbacks[1]=0;}
            else result=0;
        }
    }
    sceKernelCpuResumeIntr(intr);pspSdkSetK1(k1);return result;
}
int wifiMenuRelease(void)
{
    unsigned k1=pspSdkSetK1(0);int intr=sceKernelCpuSuspendIntr();
    /* Native rows may already exist. Never unload their action callbacks while
     * this VSH is alive. Restart VSH to change the plugin configuration. */
    int result=owner>=0 && sceKernelFindModuleByUID(owner)?-2:0;
    if(!result){control=NULL;trampoline=NULL;callbacks[0]=callbacks[1]=0;owner=-1;}
    sceKernelCpuResumeIntr(intr);pspSdkSetK1(k1);return result;
}
int wifi_menu_init(void)
{
    int intr=sceKernelCpuSuspendIntr();
    previous=sctrlHENSetStartModuleHandler(on_start);installed=1;
    sceKernelCpuResumeIntr(intr);return 0;
}
unsigned wifi_menu_poll(void)
{
    if(!control || !control->active)return 0;
    unsigned value=__sync_lock_test_and_set(&control->pending,0);
    return value<WIFI_MENU_COUNT?value:0;
}
int wifi_menu_status(void){return status;}
int wifi_menu_activity(unsigned out[3])
{
    if(!control || control->calls==activity_calls)return 0;
    int intr=sceKernelCpuSuspendIntr();
    activity_calls=control->calls;out[0]=activity_calls;out[1]=control->last_action;out[2]=control->last_argument;
    sceKernelCpuResumeIntr(intr);return 1;
}
int wifi_menu_verify(void)
{
    int intr=sceKernelCpuSuspendIntr();
    SceModule *m=owner>=0?sceKernelFindModuleByUID(owner):NULL;
    int result=-15;
    if(m && control && control->active && m->text_addr==owner_text &&
        m->text_size==WIFI_VSH_TEXT_SIZE && user_text(m,m->text_addr) &&
        m->nsegment==2 && m->segmentaddr[1]==m->text_addr+WIFI_VSH_TEXT_SIZE &&
        within(m->segmentaddr[1],m->segmentsize[1],0x08800000u,0x03800000u) &&
        m->segmentsize[1]>=WIFI_MENU_DATA_DIGEST_OFFSET+20) {
        result=menu_patch_verify((void *)m->text_addr,(void *)m->segmentaddr[1],callbacks,trampoline,m->text_addr);
        record_result(m,result);
    }
    sceKernelCpuResumeIntr(intr);return result;
}
int wifi_menu_stop(void)
{
    if(owner>=0 && sceKernelFindModuleByUID(owner))return -2;
    int intr=sceKernelCpuSuspendIntr();
    if(in_handler){sceKernelCpuResumeIntr(intr);return -2;}
    if(installed) {
        STMOD_HANDLER current=sctrlHENSetStartModuleHandler(previous);
        if(current!=on_start) {
            sctrlHENSetStartModuleHandler(current);sceKernelCpuResumeIntr(intr);return -2;
        }
        installed=0;
    }
    control=NULL;sceKernelCpuResumeIntr(intr);return 0;
}

int wifi_menu_can_stop(void){return owner<0 || !sceKernelFindModuleByUID(owner);}
