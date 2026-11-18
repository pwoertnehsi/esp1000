#include "adhoc_power_shim.h"
#include "../adhoc/aemu/esp_power.c"
static unsigned forwarded, completions;
static int submission_error;
static int (*completion)(void *,void *);
static void *completion_arg;
static int sony_done(void *packet,void *arg)
{
    assert(((unsigned char *)packet)[14]==0 && arg==(void *)13);
    ++completions;return 23;
}
static int test_async(void *packet,int flags,void *callback,void *arg)
{
    (void)packet;
    assert(flags==7);
    completion=callback;completion_arg=arg;
    ++forwarded;return submission_error ? -8 : 17;
}
int sceSysconCtrlUsbPower(signed char power)
{
    if(power){assert(owned && stub[0]==patched[0]);++power_on;return on_error;}
    assert(stub[0]==saved[0] && !calls);++power_off;return off_error;
}
int main(void)
{
    test_module=(SceModule2){.text_size=0x97c8,.version={10,1},.text_addr=(uintptr_t)text};
    unsigned *slot=&text[0x9020/4];
    slot[0]=0x08000000|(((uintptr_t)test_async>>2)&0x03ffffff);slot[1]=0;
    unsigned pristine=slot[0];
    model=0;assert(espAdhocPowerAcquire()<0);model=10;
    context=0x100;assert(espAdhocPowerAcquire()<0);context=0x200;
    usb_state=0x200;assert(espAdhocPowerAcquire()<0);usb_state=0;
    foreign=1;assert(espAdhocPowerAcquire()<0);foreign=0;
    test_module.text_size--;assert(espAdhocPowerAcquire()<0);test_module.text_size++;
    slot[1]=1;assert(espAdhocPowerAcquire()<0);slot[1]=0;
    resolve_error=1;assert(espAdhocPowerAcquire()<0);resolve_error=0;
    assert(!power_on && !power_off);
    on_error=-5;assert(espAdhocPowerAcquire()==-5);assert(!owned && slot[0]==pristine);on_error=0;
    assert(espAdhocPowerAcquire()==0 && owned && boot_waits==1);
    /* The hook remains owned after much longer than the camera timeout. */
    now+=30000000;
    assert(espAdhocPowerAcquire()==0 && boot_waits==1 && power_off==0);
    unsigned char packet[96]={0};packet[12]=0x55;packet[13]=3;
    /* Host functions need not have MIPS instruction alignment. */
    original_async=test_async;
    assert(hold_async(packet,7,(void *)sony_done,(void *)13)==17 && packet[14]==1);
    assert(forwarded==1 && calls==0 && pending==1);
    /* No rail shutdown until an outstanding Sony completion has drained. */
    assert(espAdhocPowerRelease()<0 && owned && !power_off);
    assert(completion(packet,completion_arg)==23 && packet[14]==0 && !pending && completions==1);
    submission_error=1;
    assert(hold_async(packet,7,(void *)sony_done,(void *)13)==-8);
    assert(packet[14]==0 && !pending && !callback_slot);
    submission_error=0;
    packet[12]=0x54;packet[14]=0;
    hold_async(packet,7,(void *)sony_done,(void *)13);assert(packet[14]==0);
    packet[12]=0x55;packet[13]=4;
    hold_async(packet,7,(void *)sony_done,(void *)13);assert(packet[14]==0);
    assert(completion==sony_done && completion_arg==(void *)13 && !pending);
    slot[0]^=1;assert(espAdhocPowerRelease()<0 && owned && !power_off);slot[0]^=1;
    off_error=-6;assert(espAdhocPowerRelease()==-6 && owned && slot[0]==patched[0]);off_error=0;
    calls=1;assert(espAdhocPowerRelease()<0 && owned && slot[0]==patched[0]);calls=0;
    assert(espAdhocPowerRelease()==0 && !owned && slot[0]==pristine);
    unsigned previous=power_off;assert(espAdhocPowerRelease()==0 && power_off==previous);
    packet[12]=0x55;packet[13]=3;packet[14]=0;
    assert(!esp_power_hold_packet(packet,owned) && packet[14]==0);
    assert(espAdhocPowerAcquire()==0 && boot_waits==2);
    assert(module_reboot_before(0,NULL)==0 && !owned);
    puts("Ad Hoc power ownership, ordering, filter and cleanup passed");
}
