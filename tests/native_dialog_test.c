#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../browser_local/app/native_dialog.c"
static int states[16],count,index,init_result,aborts,shutdowns,begins,ends,frames,updates;
static volatile int cancelled,finished;
static int finish_at=-1;
int sceUtilityGetSystemParamInt(int key,int *value){*value=key==PSP_SYSTEMPARAM_ID_INT_LANGUAGE?7:1;return 0;}
int sceUtilityMsgDialogInitStart(pspUtilityMsgDialogParams *p)
{
 assert(p->base.size==sizeof(*p) && p->base.language==7 && p->base.buttonSwap==1);
 assert(p->base.graphicsThread==0x11 && p->base.accessThread==0x13 && p->base.fontThread==0x12 && p->base.soundThread==0x10);
 assert(p->mode==PSP_UTILITY_MSGDIALOG_MODE_TEXT && p->options==PSP_UTILITY_MSGDIALOG_OPTION_TEXT);
 assert(!strcmp(p->message,"Connecting..."));return init_result;
}
int sceUtilityMsgDialogGetStatus(void){assert(index<count);if(index==finish_at)finished=1;return states[index++];}
void sceUtilityMsgDialogUpdate(int n){assert(n==1);++updates;}
int sceUtilityMsgDialogAbort(void){++aborts;return 0;}
void sceUtilityMsgDialogShutdownStart(void){++shutdowns;}
void browser_ui_begin(void){++begins;}
void browser_ui_frame_begin(void){++frames;assert(frames<15);}
void browser_ui_frame_end(void){}
void browser_ui_end(void){++ends;}
static void reset(void)
{count=index=aborts=shutdowns=begins=ends=frames=updates=cancelled=finished=init_result=0;finish_at=-1;}
static void sequence(int a,int b,int c,int d){states[0]=a;states[1]=b;states[2]=c;states[3]=d;count=4;}
int main(void)
{
 reset();sequence(1,2,3,4);finish_at=1;
 assert(!browser_native_dialog("Connecting...",&cancelled,&finished));
 assert(aborts==1 && shutdowns==1 && !cancelled && begins==1 && ends==1);
 reset();sequence(1,2,3,4);
 assert(!browser_native_dialog("Connecting...",&cancelled,&finished));
 assert(!aborts && shutdowns==1 && cancelled && ends==1);
 reset();sequence(1,2,3,0);cancelled=1;
 assert(!browser_native_dialog("Connecting...",&cancelled,&finished));assert(aborts==1 && ends==1);
 reset();sequence(1,2,3,4);
 assert(!browser_native_dialog("Connecting...",&cancelled,NULL));assert(!cancelled && !aborts && ends==1);
 reset();init_result=-123;assert(browser_native_dialog("Connecting...",&cancelled,&finished)==-123 && ends==1 && !frames);
 reset();sequence(1,-123,0,0);
 assert(browser_native_dialog("Connecting...",&cancelled,&finished)==-2 && !ends && !shutdowns);
 reset();states[0]=2;states[1]=2;states[2]=3;states[3]=3;states[4]=4;count=5;finish_at=0;
 assert(!browser_native_dialog("Connecting...",&cancelled,&finished));assert(aborts==1 && shutdowns==1 && ends==1);
 puts("Sony dialog language/button layout, worker completion, cancellation, init failure and retained live ownership passed");
 return 0;
}
