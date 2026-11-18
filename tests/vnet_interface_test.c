#include "../virtual_net/driver/interface.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static unsigned completions, completed_command;
static vnet_record_t *completed_record;
static int complete(vnet_record_t *r, unsigned command, int result)
{
    assert(result == 0);
    completions++; completed_record = r; completed_command = command;
    return 0;
}
static int fail_create, fail_attach, fail_detach, fail_destroy;
static unsigned creates, attaches, detaches, destroys, dequeues;
static void *dequeue(vnet_record_t *record)
{ dequeues++;record->tx_head=0;return (void *)(uintptr_t)0x1234; }
static int create(vnet_record_t *r)
{ creates++; if (fail_create) return -10; r->internal=0x12345678; return 0; }
static int attach(vnet_record_t *r,const unsigned char *mac,const char *name)
{ assert(r->internal==0x12345678 && mac[0]==2 && !strcmp(name,"usb")); attaches++; return fail_attach ? -11 : 0; }
static int detach(vnet_record_t *r)
{ assert(r->internal==0x12345678); detaches++; return fail_detach ? -12 : 0; }
static int destroy(vnet_record_t *r)
{ assert(r->internal==0x12345678 && (!attaches || detaches)); destroys++; return fail_destroy ? -13 : 0; }
static const vnet_ops_t ops={create,attach,detach,destroy};
static const unsigned char mac[6]={2,0,0,0,0,1};
static void reset(void)
{ fail_create=fail_attach=fail_detach=fail_destroy=0; creates=attaches=detaches=destroys=0; }

int main(void)
{
    vnet_session_t s={0};
    vnet_record_t other={0};
    assert(vnet_complete_address_ioctl(&s.record, &other, 2, complete) == -1);
    assert(vnet_complete_address_ioctl(&s.record, &s.record, 0xffff, complete) == -1);
    assert(completions == 0);
    for (unsigned cmd = 2; cmd <= 3; ++cmd) {
        assert(vnet_complete_address_ioctl(&s.record, &s.record, cmd, complete) == 0);
        assert(completions == cmd - 1 && completed_record == &s.record && completed_command == cmd);
    }
    assert(!vnet_dequeue_ready(&s,dequeue) && !dequeues);
    s.attached=1;
    assert(!vnet_dequeue_ready(&s,dequeue) && !dequeues);
    s.record.tx_head=0x1234;
    assert(vnet_dequeue_ready(&s,dequeue)==(void *)(uintptr_t)0x1234 && dequeues==1);
    assert(!vnet_dequeue_ready(&s,dequeue) && dequeues==1);
    s.attached=0;
    assert(vnet_attach(&s,&ops,mac,"usb")==-1);
    assert(vnet_close(&s,&ops)==0 && !creates && !destroys);
    for (unsigned i=0;i<1000;i++) {
        reset();
        assert(vnet_create(&s,&ops)==0);
        assert(vnet_create(&s,&ops)==-1);
        assert(vnet_attach(&s,&ops,mac,"usb")==0);
        assert(vnet_attach(&s,&ops,mac,"usb")==-1);
        assert(vnet_close(&s,&ops)==0);
        assert(vnet_close(&s,&ops)==0);
        assert(creates==1 && attaches==1 && detaches==1 && destroys==1 && !s.record.internal);
    }
    reset(); fail_create=1;
    assert(vnet_create(&s,&ops)==-10);
    assert(vnet_close(&s,&ops)==0 && !attaches && !destroys);
    reset(); fail_attach=1;
    assert(vnet_create(&s,&ops)==0 && vnet_attach(&s,&ops,mac,"usb")==-11);
    assert(vnet_close(&s,&ops)==0 && detaches==1 && destroys==1);
    reset(); fail_detach=1;
    assert(vnet_create(&s,&ops)==0 && vnet_attach(&s,&ops,mac,"usb")==0);
    assert(vnet_close(&s,&ops)==-12 && s.created && s.attach_attempted && !destroys);
    fail_detach=0;
    assert(vnet_close(&s,&ops)==0 && detaches==2 && destroys==1);
    reset(); fail_destroy=1;
    assert(vnet_create(&s,&ops)==0 && vnet_attach(&s,&ops,mac,"usb")==0);
    assert(vnet_close(&s,&ops)==-13 && !s.attach_attempted && s.created);
    fail_destroy=0;
    assert(vnet_close(&s,&ops)==0 && detaches==1 && destroys==2);
    reset(); fail_detach=1;
    assert(vnet_create(&s,&ops)==0 && vnet_attach(&s,&ops,mac,"usb")==0);
    assert(vnet_close(&s,&ops)==-12);
    /* Native sceNetTerm owns pool destruction on this recovery path. */
    vnet_forget_after_netterm(&s);
    assert(vnet_close(&s,&ops)==0 && !destroys && !s.record.internal);
    puts("PASS: virtual interface ownership, 1000 lifecycles, partial failures and pool teardown");
    return 0;
}
