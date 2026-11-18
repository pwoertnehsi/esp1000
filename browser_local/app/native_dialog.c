#include <pspkernel.h>
#include <psputility.h>
#include <psputility_msgdialog.h>
#include <psputility_sysparam.h>
#include <string.h>
#include "native_dialog.h"
int browser_native_dialog(const char *message,volatile int *cancelled,volatile int *finished)
{
    pspUtilityMsgDialogParams params;memset(&params,0,sizeof(params));
    params.base.size=sizeof(params);params.base.language=PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,&params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP,&params.base.buttonSwap);
    params.base.graphicsThread=0x11;params.base.accessThread=0x13;
    params.base.fontThread=0x12;params.base.soundThread=0x10;
    params.mode=PSP_UTILITY_MSGDIALOG_MODE_TEXT;params.options=PSP_UTILITY_MSGDIALOG_OPTION_TEXT;
    strncpy(params.message,message,sizeof(params.message)-1);
    browser_ui_begin();
    int result=sceUtilityMsgDialogInitStart(&params);
    if(result<0){browser_ui_end();return result;}
    int aborted=0,shutdown=0;
    for(;;) {
        browser_ui_frame_begin();int state=sceUtilityMsgDialogGetStatus();
        if(state<0)return -2; /* Keep utility/GU ownership on an unknown state. */
        if(state==PSP_UTILITY_DIALOG_VISIBLE) {
            sceUtilityMsgDialogUpdate(1);
            if(!aborted && (*cancelled || (finished && *finished))) {
                if(sceUtilityMsgDialogAbort()>=0)aborted=1;
            }
        } else if(state==PSP_UTILITY_DIALOG_QUIT && !shutdown) {
            if(!aborted && finished && !*finished)*cancelled=1;
            sceUtilityMsgDialogShutdownStart();shutdown=1;
        } else if(state==PSP_UTILITY_DIALOG_NONE || state==PSP_UTILITY_DIALOG_FINISHED) {
            browser_ui_end();return 0;
        }
        browser_ui_frame_end();
    }
}
