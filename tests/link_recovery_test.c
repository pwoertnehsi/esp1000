#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../common/link_recovery.h"
int main(void)
{
    link_recovery_t s={0};
    assert(!link_recovery_poll(&s,0,1));
    assert(link_recovery_poll(&s,10,0)==1);
    assert(link_recovery_poll(&s,PSP_LINK_RECOVERY_US+9,0)==1);
    assert(link_recovery_poll(&s,PSP_LINK_RECOVERY_US+10,0)==-1);
    assert(!link_recovery_poll(&s,PSP_LINK_RECOVERY_US+11,1));
    assert(link_recovery_poll(&s,UINT32_MAX-10,0)==1);
    assert(link_recovery_poll(&s,20,0)==1); /* Clock wrap. */
    assert(!link_recovery_poll(&s,21,1));
    assert(link_recovery_poll(&s,22,0)==1 && s.since==22);
    puts("PASS: bounded transient link retention, real readiness, fresh recovery budget and clock wrap");
}
