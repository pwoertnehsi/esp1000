#ifndef ESP_WIFI_MENU_CONTROL_H
#define ESP_WIFI_MENU_CONTROL_H
#include <stdint.h>
#define WIFI_MENU_MAGIC 0x574d4e31u
#define WIFI_MENU_ABI 2u
/* Commands are bounded integers, never user-supplied paths or strings. */
enum {WIFI_MENU_NONE=0,WIFI_MENU_BROWSER=1,WIFI_MENU_SETTINGS=2,WIFI_MENU_RSS_SETTINGS=3,
      WIFI_MENU_GAME_SHARING=4,WIFI_MENU_MANUAL=5,WIFI_MENU_LOCATIONFREE=6,WIFI_MENU_SKYPE=7,
      WIFI_MENU_REMOTE_PLAY=8,WIFI_MENU_RADIO=9,WIFI_MENU_RSS=10,WIFI_MENU_SEARCH=11,
      WIFI_MENU_SIGNUP=12,WIFI_MENU_STORE=13,WIFI_MENU_INFORMATION=14,WIFI_MENU_COUNT=15};
typedef struct {
    uint32_t magic,abi;
    volatile uint32_t active,pending;
    uint32_t original[2];
    volatile uint32_t calls,last_action,last_argument;
} wifi_menu_control_t;
/* Native Settings, RSS and other services remain in Sony's own VSH pages.
 * Only the working browser session needs the standalone transport launcher. */
static inline int wifi_menu_uses_launcher(unsigned command)
{return command==WIFI_MENU_BROWSER || command==WIFI_MENU_MANUAL;}
static inline unsigned wifi_menu_command(int action,int argument)
{
    if(action==3 && argument==3)return WIFI_MENU_SETTINGS;
    if(action==2 && argument==9)return WIFI_MENU_RSS_SETTINGS;
    if(action==12 && argument==0)return WIFI_MENU_GAME_SHARING;
    if(action==16 && argument==0)return WIFI_MENU_BROWSER;
    if(action==22 && argument==2)return WIFI_MENU_MANUAL;
    if(action==18 && argument==0)return WIFI_MENU_LOCATIONFREE;
    if(action==24 && argument==0)return WIFI_MENU_SKYPE;
    if(action==30 && argument==18)return WIFI_MENU_REMOTE_PLAY;
    if(action==27 && argument==0)return WIFI_MENU_RADIO;
    if(action==8 && argument==11)return WIFI_MENU_RSS;
    if(action==29 && argument==6)return WIFI_MENU_SEARCH;
    if(action==30 && argument==1)return WIFI_MENU_SIGNUP;
    if(action==30 && argument==3)return WIFI_MENU_STORE;
    if(action==30 && argument==8)return WIFI_MENU_INFORMATION;
    return WIFI_MENU_NONE;
}
#endif
