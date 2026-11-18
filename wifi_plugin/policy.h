#ifndef PSP_WIFI_PLUGIN_POLICY_H
#define PSP_WIFI_PLUGIN_POLICY_H
#include <stdint.h>
#define WIFI_SHORTCUT_HOLD_US 1000000u
typedef struct {uint32_t since;int held,latched;} wifi_shortcut_t;
/* Busy periods consume the chord too: leaving storage while still holding it
 * must never cause a delayed launch. Release and press again to retry. */
static inline int wifi_shortcut_update(wifi_shortcut_t *s,uint32_t now,int chord,int allowed)
{
    if(!chord){s->held=s->latched=0;return 0;}
    if(!allowed){s->latched=1;return 0;}
    if(!s->held){s->held=1;s->since=now;return 0;}
    if(s->latched || now-s->since<WIFI_SHORTCUT_HOLD_US)return 0;
    s->latched=1;return 1;
}
#endif
