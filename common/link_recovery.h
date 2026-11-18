#ifndef PSP_LINK_RECOVERY_H
#define PSP_LINK_RECOVERY_H
#include <stdint.h>
#define PSP_LINK_RECOVERY_US 30000000u
typedef struct {unsigned since;int waiting;} link_recovery_t;
/* Live sessions keep their resources during a bounded transient loss. 'ready'
 * remains a separate truthful transport state. Unsigned subtraction wraps. */
static inline int link_recovery_poll(link_recovery_t *s,unsigned now,int ready)
{
    if(ready){s->waiting=0;return 0;}
    if(!s->waiting){s->waiting=1;s->since=now;return 1;}
    return now-s->since<PSP_LINK_RECOVERY_US?1:-1;
}
#endif
