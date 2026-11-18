#ifndef PSP_VNET_INTERFACE_H
#define PSP_VNET_INTERFACE_H
#include <stdint.h>
#include <stddef.h>

/* Private 6.61 ABI, recovered from the supplied ifhandle/WLAN call sites.
 * External record is distinct from the native allocation in internal. */
typedef struct {
    uint32_t internal;
    uint32_t context;
    uint32_t up;
    uint32_t down;
    uint32_t send;
    uint32_t ioctl;
    volatile uint32_t tx_head;
    uint32_t tx_tail, tx_count;
    uint32_t reserved_24, reserved_28;
} vnet_record_t;
_Static_assert(sizeof(vnet_record_t) == 0x2c, "external interface record ABI");
_Static_assert(offsetof(vnet_record_t, ioctl) == 0x14, "ioctl callback ABI");
_Static_assert(offsetof(vnet_record_t, tx_head) == 0x18, "native TX queue ABI");

typedef struct {
    int (*create)(vnet_record_t *);
    int (*attach)(vnet_record_t *, const unsigned char *, const char *);
    int (*detach)(vnet_record_t *);
    int (*destroy)(vnet_record_t *);
} vnet_ops_t;

typedef struct {
    vnet_record_t record;
    int created;
    int attach_attempted;
    int attached;
} vnet_session_t;

void *vnet_dequeue_ready(vnet_session_t *, void *(*dequeue)(vnet_record_t *));

/* NET increments its completion count before address ioctls 2/3. A return
 * value alone never wakes its semaphore waiter. */
int vnet_complete_address_ioctl(vnet_record_t *expected, vnet_record_t *actual,
    unsigned command, int (*complete)(vnet_record_t *, unsigned, int));

int vnet_create(vnet_session_t *session, const vnet_ops_t *ops);
int vnet_attach(vnet_session_t *session, const vnet_ops_t *ops,
                const unsigned char mac[6], const char *name);
int vnet_close(vnet_session_t *session, const vnet_ops_t *ops);
/* Only after the owning launcher has successfully terminated native NET.
 * Its pool/registry are then gone; do not call native detach/free again. */
void vnet_forget_after_netterm(vnet_session_t *session);
#endif
