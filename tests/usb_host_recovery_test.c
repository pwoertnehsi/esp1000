#include <assert.h>
#include <stdio.h>
#include "../common/usb_host_recovery.h"
int main(void)
{
    assert(!usb_host_recovery_eligible(0,0,0));
    assert(usb_host_recovery_eligible(0,1,0));
    assert(usb_host_recovery_eligible(1,1,0));
    assert(!usb_host_recovery_eligible(0,1,1));
    usb_host_recovery_t s={0};
    /* Unknown devices, disabled power, disconnection and live sessions are inert. */
    assert(!usb_host_recovery_poll(&s,0,1,0,0,1));
    assert(!usb_host_recovery_poll(&s,100000,1,0,0,1) && !s.attempts);
    assert(!usb_host_recovery_poll(&s,200000,1,1,1,1));
    assert(!usb_host_recovery_poll(&s,300000,1,1,1,1) && !s.waiting);
    assert(!usb_host_recovery_poll(&s,300001,1,1,0,1));
    assert(!usb_host_recovery_poll(&s,315000,1,1,0,1));
    assert(usb_host_recovery_poll(&s,315001,1,1,0,1) && s.attempts==1);
    usb_host_recovery_restarted(&s,315301);
    assert(!usb_host_recovery_poll(&s,315302,0,1,0,1) && s.attempts==1);
    assert(!usb_host_recovery_poll(&s,315302,1,1,0,1)); /* Host restart delay. */
    assert(usb_host_recovery_poll(&s,330001,1,1,0,1) && s.attempts==2);
    assert(usb_host_recovery_poll(&s,345001,1,1,0,1) && s.attempts==3);
    assert(!usb_host_recovery_poll(&s,999999,1,1,0,1) && s.attempts==3);
    assert(!usb_host_recovery_poll(&s,1000000,0,1,0,1) && !s.attempts && !s.waiting);
    assert(!usb_host_recovery_poll(&s,1000001,1,1,0,1));
    assert(!usb_host_recovery_poll(&s,1015001,1,1,0,0) && !s.attempts);
    assert(!usb_host_recovery_poll(&s,1030001,1,1,0,1));
    assert(usb_host_recovery_poll(&s,1045001,1,1,0,1));
    assert(!usb_host_recovery_poll(&s,1045002,1,1,1,1) && !s.attempts);
    puts("PASS: known-PSP enumeration timeout, bounded retries, healthy idle exclusion and fresh reconnect budgets");
}
