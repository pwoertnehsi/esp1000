#define _GNU_SOURCE
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "../wifi_plugin/menu_runtime.c"
static SceModule vsh,bridge,exit_module;
static int exit_present;
static STMOD_HANDLER handler;
static int caches,prior_calls,vsh_present=1;
static unsigned k1=0x1234;
unsigned pspSdkSetK1(unsigned value){unsigned old=k1;k1=value;return old;}
int sceKernelCpuSuspendIntr(void){return 1;}
void sceKernelCpuResumeIntr(int old){assert(old==1);}
void sceKernelDcacheWritebackInvalidateAll(void){++caches;}
void sceKernelIcacheInvalidateAll(void){++caches;}
SceModule *sceKernelFindModuleByName(const char *name)
{if(!strcmp(name,"vsh_module"))return vsh_present?&vsh:NULL;if(!strcmp(name,"ESP WiFi XMB Bridge"))return &bridge;return NULL;}
SceModule *sceKernelFindModuleByUID(int uid){return uid==vsh.modid && vsh_present?&vsh:uid==45 && exit_present?&exit_module:NULL;}
SceModule *sceKernelFindModuleByAddress(unsigned address)
{return user_text(&vsh,address)?&vsh:user_text(&bridge,address)?&bridge:NULL;}
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER next){STMOD_HANDLER old=handler;handler=next;return old;}
static int prior(SceModule *m){(void)m;++prior_calls;return 17;}
static uint32_t *test_trampoline;
static void seed(uint32_t *text)
{
 memset(text,0,WIFI_VSH_TEXT_SIZE);
 memset((unsigned char *)text+WIFI_VSH_TEXT_SIZE,0,30176);
 text[WIFI_MENU_COLUMN_BRANCH/4]=WIFI_MENU_COLUMN_ORIGINAL;
 for(unsigned i=0;i<MENU_LENGTH(menu_column_anchors);++i)text[menu_column_anchors[i].offset/4]=menu_column_anchors[i].word;
 memcpy(test_trampoline,menu_trampoline_empty,16);
 memcpy(text+WIFI_MENU_ACTION_ENTRY/4,menu_entry_original,8);
 for(unsigned i=0;i<MENU_LENGTH(menu_resources);++i)memcpy(text+menu_resources[i].offset/4,menu_resources[i].words,20);
 memcpy(text+WIFI_MENU_TABLE_OFFSET/4,menu_visibility_original,sizeof(menu_visibility_original));
 memcpy((unsigned char *)text+WIFI_VSH_TEXT_SIZE+WIFI_MENU_DATA_DIGEST_OFFSET,menu_digest_original,20);
 for(unsigned i=0;i<MENU_LENGTH(menu_anchors);++i)text[menu_anchors[i].offset/4]=menu_anchors[i].word;
 for(unsigned i=0;i<MENU_LENGTH(menu_rows);++i){uint32_t *p=text+menu_rows[i].offset/4;
  p[0]=menu_rows[i].category;p[1]=menu_rows[i].item;p[2]=menu_rows[i].mask;p[3]=menu_rows[i].region;}
 for(unsigned i=0;i<2;++i)text[menu_call_offsets[i]/4]=0x0c000000|(((vsh.text_addr+0x16a70)>>2)&0x3ffffff);
}
/* Execute the emitted displaced instructions in a minimal independent MIPS
 * model: stack frame, saved register, arguments and caller RA must survive. */
static void check_trampoline(const uint32_t code[4],uint32_t origin)
{
 uint32_t regs[32]={0},saved_address=0,saved_value=0,next=0;
 regs[29]=0x09ff0000;regs[19]=0x12345678;regs[31]=0x0a010000;regs[4]=3;regs[5]=3;
 for(unsigned i=0;i<4;++i){uint32_t w=code[i],op=w>>26;
  if(!w)continue;
  if(op==9){unsigned rs=(w>>21)&31,rt=(w>>16)&31;regs[rt]=regs[rs]+(int16_t)w;}
  else if(op==43){unsigned rs=(w>>21)&31,rt=(w>>16)&31;saved_address=regs[rs]+(int16_t)w;saved_value=regs[rt];}
  else if(op==2){next=(w&0x03ffffffu)<<2;}
  else assert(!"Unexpected relocated instruction");
 }
 assert(next==origin+8 && regs[29]==0x09feffe0 && saved_address==regs[29]+12 && saved_value==regs[19]);
 assert(regs[4]==3 && regs[5]==3 && regs[31]==0x0a010000);
}
#ifndef VSH_TEST_BASE
#define VSH_TEST_BASE 0x0a040f00u
#endif
static void check_browser_exit(void)
{
 unsigned *text=mmap((void *)0x09400000,0x26000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
 assert(text!=MAP_FAILED);
 SceModule html={0};strcpy(html.modname,"htmlviewer_plugin_module");html.version[0]=html.version[1]=1;html.modid=45;
 html.text_addr=html.segmentaddr[0]=0x09400000;html.text_size=html.segmentsize[0]=0x1f700;html.nsegment=2;html.segmentaddr[1]=html.text_addr+html.text_size;html.segmentsize[1]=21484;
 text[0x296c/4]=0x27bdfff0;text[0x29fc/4]=exit_jal(html.text_addr+0x299c)^0x04000000u;text[0x2a00/4]=0x8fbf0004;
 for(unsigned i=0;i<WIFI_EXIT_STAGES;++i){control->exit_callbacks[i]=bridge.text_addr+0x100+i*4;text[browser_exit_calls[i].call/4]=exit_jal(html.text_addr+browser_exit_calls[i].target);text[browser_exit_calls[i].call/4+1]=browser_exit_delay(i,html.segmentaddr[1]);}
 control->exit_owner=-1;
 assert(browser_exit_delay(0,html.segmentaddr[1])==0xa045fd80u);
 text[0x29c8/4]=0xa0450680;observe_exit(&html);assert(control->exit_status==-120);
 text[0x29c8/4]=browser_exit_delay(0,html.segmentaddr[1]);
 unsigned last=browser_exit_calls[WIFI_EXIT_STAGES-1].call/4,first=text[browser_exit_calls[0].call/4];text[last]^=4;
 observe_exit(&html);assert(control->exit_status<0 && text[browser_exit_calls[0].call/4]==first);
 text[last]^=4;observe_exit(&html);assert(control->exit_status==1 && control->exit_owner==45);
 for(unsigned i=0;i<WIFI_EXIT_STAGES;++i){assert(control->exit_originals[i]==html.text_addr+browser_exit_calls[i].target);assert(text[browser_exit_calls[i].call/4]==exit_jal(control->exit_callbacks[i]));assert(text[browser_exit_calls[i].call/4+1]==browser_exit_delay(i,html.segmentaddr[1]));}
 observe_exit(&html);assert(control->exit_status==1);
 exit_present=1;vsh_present=0;assert(!wifi_menu_can_stop() && wifiMenuRelease()==-2 && wifi_menu_stop()==-2);exit_present=0;vsh_present=1;
 munmap(text,0x26000);
}
int main(void)
{
 void *memory=mmap((void *)(VSH_TEST_BASE&~0xfffu),0x100000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 assert(memory==(void *)(VSH_TEST_BASE&~0xfffu));
 void *bridge_mem=mmap((void *)0x09000000,0x10000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 assert(bridge_mem==(void *)0x09000000);
 strcpy(vsh.modname,"vsh_module");vsh.modid=1;vsh.version[0]=1;vsh.version[1]=1;
 vsh.text_addr=VSH_TEST_BASE;vsh.text_size=WIFI_VSH_TEXT_SIZE;
 vsh.nsegment=2;vsh.segmentaddr[0]=vsh.text_addr;vsh.segmentsize[0]=WIFI_VSH_TEXT_SIZE;
 vsh.segmentaddr[1]=vsh.text_addr+WIFI_VSH_TEXT_SIZE;vsh.segmentsize[1]=30176;
 bridge.text_addr=0x09000000;bridge.text_size=0x1000;bridge.nsegment=1;
 bridge.segmentaddr[0]=0x09000000;bridge.segmentsize[0]=0x10000;
 wifi_menu_control_t *request=(void *)0x09002000;request->magic=WIFI_MENU_MAGIC;request->abi=WIFI_MENU_ABI;
 test_trampoline=(void *)0x09000400;
 request->original[0]=request->original[1]=0x09000400;
 uint32_t cb[2]={0x09000100,0x09000200};uint32_t *text=(void *)((char *)memory+(VSH_TEST_BASE&0xfffu));
 unsigned char *data=(unsigned char *)text+WIFI_VSH_TEXT_SIZE;
 SceModule bounds={0};bounds.text_addr=0x08800000;bounds.text_size=4;
 assert(user_text(&bounds,0x08800000));
 bounds.text_addr=0x0bfffffc;assert(user_text(&bounds,0x0bfffffc));
 bounds.text_size=8;assert(!user_text(&bounds,0x0bfffffc));
 bounds.text_addr=0x88000000;bounds.text_size=4;assert(!user_text(&bounds,0x88000000));
 bounds.text_addr=0x08800000;bounds.text_size=0xffffffffu;assert(!user_text(&bounds,0x08800000));
 seed(text);uint32_t snapshot[WIFI_VSH_TEXT_SIZE/4];memcpy(snapshot,text,sizeof(snapshot));
 assert(menu_patch_prepare(text,WIFI_VSH_TEXT_SIZE-4,vsh.text_addr,cb,data,30176,test_trampoline,0x09000400)<0);
 for(unsigned i=0;i<MENU_LENGTH(menu_rows);++i)for(unsigned j=0;j<4;++j){
  text[menu_rows[i].offset/4+j]^=1;assert(menu_patch_prepare(text,WIFI_VSH_TEXT_SIZE,vsh.text_addr,cb,data,30176,test_trampoline,0x09000400)<0);
  text[menu_rows[i].offset/4+j]^=1;
 }
 for(unsigned i=0;i<MENU_LENGTH(menu_anchors);++i){text[menu_anchors[i].offset/4]^=1;
  assert(menu_patch_prepare(text,WIFI_VSH_TEXT_SIZE,vsh.text_addr,cb,data,30176,test_trampoline,0x09000400)<0);text[menu_anchors[i].offset/4]^=1;}
 for(unsigned i=0;i<MENU_LENGTH(menu_resources);++i)for(unsigned j=0;j<5;++j) {
  text[menu_resources[i].offset/4+j]^=1;
  assert(menu_patch_prepare(text,WIFI_VSH_TEXT_SIZE,vsh.text_addr,cb,data,30176,test_trampoline,0x09000400)<0);
  text[menu_resources[i].offset/4+j]^=1;
 }
 for(unsigned i=0;i<MENU_LENGTH(menu_column_anchors);++i) {
  text[menu_column_anchors[i].offset/4]^=1;
  assert(menu_patch_prepare(text,WIFI_VSH_TEXT_SIZE,vsh.text_addr,cb,data,30176,test_trampoline,0x09000400)<0);
  text[menu_column_anchors[i].offset/4]^=1;
 }
 assert(!memcmp(snapshot,text,sizeof(snapshot)));
 handler=prior;assert(!wifi_menu_init());assert(handler==on_start);
 assert(wifiMenuRegister(0x09002001,sizeof(*request),cb[0],cb[1])<0 && !control);
 assert(wifiMenuRegister(0x09002000,sizeof(*request)-4,cb[0],cb[1])<0 && !control);
 assert(wifiMenuRegister(0x09002000,sizeof(*request),0x09001000,cb[1])<0 && !control);
 request->original[0]=request->original[1]=0x09001000;
 assert(wifiMenuRegister(0x09002000,sizeof(*request),cb[0],cb[1])<0 && !control && !caches);
 request->original[0]=request->original[1]=0x09000400;
 test_trampoline[0]^=1;
 assert(wifiMenuRegister(0x09002000,sizeof(*request),cb[0],cb[1])<0 && !control && !caches);
 test_trampoline[0]^=1;
 /* A foreign entry hook must fail without any write. */
 text[WIFI_MENU_ACTION_ENTRY/4]=0x08000000|((0x09200000>>2)&0x3ffffff);
 assert(wifiMenuRegister(0x09002000,sizeof(*request),cb[0],cb[1])<0 && !control && !caches);
 seed(text);
 /* Reproduce v0.4's -11 without conflating version and address checks. */
 vsh.version[0]=2;
 assert(wifiMenuRegister(0x09002000,sizeof(*request),cb[0],cb[1])==-11 && !control && !caches);
 wifi_menu_snapshot_t shot;wifi_menu_snapshot(&shot);
 assert(shot.result==-11 && shot.version[0]==2 && shot.user_address && shot.text==vsh.text_addr);
 assert(!memcmp(snapshot,text,sizeof(snapshot)));
 vsh.version[0]=1;vsh.text_addr=0x08400000;
 assert(wifiMenuRegister(0x09002000,sizeof(*request),cb[0],cb[1])==-11 && !control && !caches);
 wifi_menu_snapshot(&shot);assert(!shot.user_address && shot.text==0x08400000 && !shot.anchor_count);
 vsh.text_addr=VSH_TEST_BASE;
 vsh.nsegment=2;vsh.segmentaddr[0]=vsh.text_addr;vsh.segmentsize[0]=WIFI_VSH_TEXT_SIZE;
 /* CFW call hooks remain untouched. All original entry callers reach us. */
 text[menu_call_offsets[1]/4]=0x0c000000|((0x09000300>>2)&0x3ffffff);
 snapshot[menu_call_offsets[1]/4]=text[menu_call_offsets[1]/4];
 assert(!wifiMenuRegister(0x09002000,sizeof(*request),cb[0],cb[1]));assert(k1==0x1234);
 assert(!memcmp(data+WIFI_MENU_DATA_DIGEST_OFFSET,menu_digest_modified,20));
 assert(request->active && caches==2 && request->original[0]==0x09000400 && request->original[1]==0x09000400);
 for(unsigned i=0;i<MENU_LENGTH(menu_rows);++i){uint32_t *p=text+menu_rows[i].offset/4;
  assert(p[0]==menu_rows[i].category && p[1]==menu_rows[i].item && p[2]==(menu_rows[i].mask|0x400) && p[3]==menu_rows[i].region);}
 for(unsigned i=0;i<WIFI_VSH_TEXT_SIZE/4;++i) {
  int changed=0;
  for(unsigned j=0;j<MENU_LENGTH(menu_rows);++j)if(i==(menu_rows[j].offset+8)/4)changed=1;
  for(unsigned j=0;j<MENU_LENGTH(menu_resources);++j)if(i==(menu_resources[j].offset+12)/4)changed=1;
  if(i==WIFI_MENU_COLUMN_BRANCH/4)changed=1;
  if(i==WIFI_MENU_ACTION_ENTRY/4 || i==WIFI_MENU_ACTION_ENTRY/4+1)changed=1;
  if(!changed)assert(text[i]==snapshot[i]);
 }
 wifi_menu_snapshot(&shot);assert(shot.result==1 && shot.anchor_count==MENU_LENGTH(menu_anchors) && shot.row_count==MENU_LENGTH(menu_rows));
 for(unsigned i=0;i<shot.anchor_count;++i)assert(shot.anchor_words[i]==(menu_anchors[i].offset==WIFI_MENU_ACTION_ENTRY?(0x08000000u|((cb[0]>>2)&0x03ffffffu)):menu_anchors[i].word));
 for(unsigned i=0;i<shot.row_count;++i)assert(shot.row_masks[i]==(menu_rows[i].mask|0x400));
 assert(handler(&vsh)==17 && prior_calls==1 && caches==2);
 assert(wifi_menu_verify()==2 && wifi_menu_status()==2 && caches==2);
 /* Reproduce ARK installing both action call hooks after our module prologue.
  * Entry routing and the original trampoline must survive unchanged. */
 for(unsigned i=0;i<2;++i)text[menu_call_offsets[i]/4]=0x0c000000u|((0x09000300u>>2)&0x3ffffffu);
 assert(wifi_menu_verify()==2 && caches==2);
 assert(test_trampoline[0]==0x27bdffe0 && test_trampoline[1]==0xafb3000c);
 assert(test_trampoline[2]==(0x08000000u|(((vsh.text_addr+WIFI_MENU_ACTION_ENTRY+8)>>2)&0x3ffffffu)));
 assert(!test_trampoline[3]);
 check_trampoline(test_trampoline,vsh.text_addr+WIFI_MENU_ACTION_ENTRY);

 /* Read-only live-view trace distinguishes item creation from missing
  * root/resource/UI columns, without changing a PAF object to force visibility. */
 void *view=mmap((void *)0x09200000,0x10000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 assert(view==(void *)0x09200000);
 *(uint32_t *)(data+0x618c)=0x09200000;
 *(uint32_t *)((char *)view+0xa6c)=0x09204000;
 *(uint32_t *)((char *)view+0x4000+0x334)=8;
 for(unsigned i=0;i<8;++i) {
  *(uint32_t *)((char *)view+0xe8c+12*i)=i+1;
  *(uint32_t *)(data+0x61b8+88*i)=10+i;
  *(uint32_t *)(data+0x7028+88*i)=20+i;
 }
 assert(wifi_menu_verify()==2);wifi_menu_snapshot(&shot);
 assert(shot.menu_instance==0x09200000 && shot.menu_parent==0x09204000 && shot.category_count==8);
 for(unsigned i=0;i<8;++i)assert(shot.item_counts[i]==i+1 && shot.root_resources[i]==10+i && shot.network_resources[i]==20+i);
 *(uint32_t *)(data+0x618c)=0x88000000;
 assert(wifi_menu_verify()==2);wifi_menu_snapshot(&shot);assert(!shot.menu_instance && !shot.menu_parent);
 *(uint32_t *)(data+0x618c)=0;
 /* Readback detects restoration/drift and never silently reapplies writes. */
 text[WIFI_MENU_TABLE_OFFSET/4]^=1;
 assert(wifi_menu_verify()==-14 && (text[WIFI_MENU_TABLE_OFFSET/4]&1) && caches==2);
 text[WIFI_MENU_TABLE_OFFSET/4]^=1;assert(wifi_menu_verify()==2);

 request->native_request=0;
 assert(!wifi_menu_launch(WIFI_MENU_BROWSER) && request->native_request==WIFI_MENU_BROWSER);
 assert(wifi_menu_launch(WIFI_MENU_MANUAL)==-2 && request->native_request==WIFI_MENU_BROWSER);
 assert(wifi_menu_launch(WIFI_MENU_SETTINGS)==-1);
 request->native_request=0;
 request->pending=WIFI_MENU_SETTINGS;assert(wifi_menu_poll()==WIFI_MENU_SETTINGS && !request->pending);
 request->pending=999;assert(!wifi_menu_poll());
 assert(wifiMenuRelease()<0 && wifi_menu_stop()<0 && control && handler==on_start);
 vsh_present=0;assert(!wifiMenuRelease() && !control);
 assert(!wifi_menu_stop() && handler==prior);
 /* Registration before VSH startup installs routing when the module arrives. */
 seed(text);request->active=0;request->pending=0;assert(!wifi_menu_init());
 assert(!wifiMenuRegister(0x09002000,sizeof(*request),cb[0],cb[1]) && !request->active);
 vsh_present=1;assert(handler(&vsh)==17 && request->active);
 check_browser_exit();
 puts("XMB guards, atomic preflight, user registration, CFW chaining and unload refusal passed");
 return 0;
}
