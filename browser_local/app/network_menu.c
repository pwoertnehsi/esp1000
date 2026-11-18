/* Backend status UI for restored native XMB rows. The outer XMB uses Sony's
 * existing items/assets. This companion is deliberately labelled a prototype;
 * it must not claim that ESP infrastructure transport implements PSP IBSS. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include "network_menu.h"
#include "../../wifi_plugin/menu_control.h"
static const char *titles[WIFI_MENU_COUNT]={"","Internet Browser","Network Settings","RSS Channel Settings",
    "Game Sharing","Online Instruction Manuals","LocationFree Player","Skype","Remote Play",
    "Internet Radio","RSS Channel","Internet Search","PlayStation Network Sign Up","PlayStation Store","Information Board"};
static const char *status(unsigned command)
{
    switch(command) {
    case WIFI_MENU_GAME_SHARING:return "Game Sharing needs PSP ad hoc transport.\nESP infrastructure routing does not provide it yet.";
    case WIFI_MENU_RSS_SETTINGS:case WIFI_MENU_RSS:return "RSS menus need a separate native player integration.\nThe current transport is integrated with Internet Browser.";
    case WIFI_MENU_LOCATIONFREE:return "LocationFree player integration is not implemented.";
    case WIFI_MENU_SKYPE:return "The historical Skype entry is restored for menu parity.\nNo Skype service backend is implemented.";
    case WIFI_MENU_REMOTE_PLAY:return "PS3 discovery, pairing and Remote Play are not implemented.";
    case WIFI_MENU_RADIO:return "Native Internet Radio integration is not implemented.";
    case WIFI_MENU_SEARCH:return "Native Internet Search integration is not implemented.\nUse Internet Browser for now.";
    default:return "This historical online service is not implemented.\nRestoring an XMB icon does not restore its service.";
    }
}
int network_menu(unsigned command,volatile int *exiting)
{
    if(command==WIFI_MENU_BROWSER || command==WIFI_MENU_MANUAL)return 1;
    if(command==0 || command>=WIFI_MENU_COUNT)return 0;
    enum {ROOT,ADHOC,INFRA,PROFILE,NEW_CONNECTION,NOTICE};
    int screen=ROOT,selection=0,redraw=1,return_screen=ROOT;
    unsigned previous=~0u;
    const char *notice=NULL;
    if(command!=WIFI_MENU_SETTINGS){screen=NOTICE;notice=status(command);return_screen=-1;}
    while(!*exiting) {
        if(redraw) {
            pspDebugScreenClear();
            pspDebugScreenPrintf("%s\nESP WiFi menu prototype 0.4\n\n",titles[command]);
            const char *rows[4];int count=0;
            if(screen==ROOT){rows[0]="Ad Hoc Mode";rows[1]="Infrastructure Mode";count=2;}
            if(screen==ADHOC){rows[0]="Automatic";rows[1]="Ch 1";rows[2]="Ch 6";rows[3]="Ch 11";count=4;
                pspDebugScreenPrintf("Channel selection (PSP ad hoc backend pending)\n\n");}
            if(screen==INFRA){rows[0]="ESP adapter (preconfigured)";rows[1]="[New Connection]";count=2;}
            if(screen==PROFILE){rows[0]="Connect and open Internet Browser";rows[1]="Test Connection";count=2;
                pspDebugScreenPrintf("USB virtual Ethernet -> ESP -> configured WiFi\nIP/DNS: supplied by the existing ESP adapter\nNo Sony WLAN profile is written.\n\n");}
            if(screen==NEW_CONNECTION){rows[0]="Scan";rows[1]="Enter Manually";rows[2]="Automatic (regional feature; unavailable)";count=3;}
            if(screen==NOTICE)pspDebugScreenPrintf("%s\n",notice);
            for(int i=0;i<count;++i)pspDebugScreenPrintf("%s %s\n",i==selection?">":" ",rows[i]);
            pspDebugScreenPrintf("\nUP/DOWN: select  CROSS: enter  CIRCLE: back\n");redraw=0;
        }
        SceCtrlData pad;sceCtrlPeekBufferPositive(&pad,1);
        unsigned pressed=pad.Buttons&~previous;previous=pad.Buttons;
        int count=screen==ADHOC?4:screen==NEW_CONNECTION?3:screen==NOTICE?0:2;
        if(count && (pressed&(PSP_CTRL_UP|PSP_CTRL_DOWN))) {
            selection=(selection+((pressed&PSP_CTRL_UP)?count-1:1))%count;redraw=1;
        }
        if(pressed&PSP_CTRL_CIRCLE) {
            if(screen==ROOT || (screen==NOTICE && return_screen<0))return 0;
            screen=screen==NOTICE?return_screen:(screen==PROFILE || screen==NEW_CONNECTION)?INFRA:ROOT;
            selection=0;redraw=1;
        } else if((pressed&PSP_CTRL_CROSS) && count) {
            if(screen==ROOT){screen=selection?INFRA:ADHOC;selection=0;}
            else if(screen==INFRA){screen=selection?NEW_CONNECTION:PROFILE;selection=0;}
            else if(screen==PROFILE)return selection?2:1;
            else {
                return_screen=screen;
                notice=screen==ADHOC?"PSP IBSS/ad hoc transport is not implemented.\nChannel was not saved or applied.\nGame Sharing and ad hoc games remain unavailable.":
                    "ESP scan/provisioning is not implemented yet.\nConfigure the ESP using the existing project workflow.\nNo connection or password was saved.";
                screen=NOTICE;
            }
            redraw=1;
        }
        sceDisplayWaitVblankStart();sceKernelDelayThreadCB(20000);
    }
    return 0;
}
