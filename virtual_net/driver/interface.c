#include "interface.h"
#include <string.h>

void *vnet_dequeue_ready(vnet_session_t *session, void *(*dequeue)(vnet_record_t *))
{
    if (!session->attached || !session->record.tx_head) return NULL;
    return dequeue(&session->record);
}

int vnet_create(vnet_session_t *session, const vnet_ops_t *ops)
{
    if (session->created || session->attach_attempted) return -1;
    memset(&session->record, 0, sizeof(session->record));
    int r = ops->create(&session->record);
    if (r == 0) session->created = 1;
    return r;
}

int vnet_attach(vnet_session_t *session, const vnet_ops_t *ops,
                const unsigned char mac[6], const char *name)
{
    if (!session->created || session->attach_attempted || !session->record.internal)
        return -1;
    /* An error can follow partial native registration. Keep the record until
     * native detach acknowledges cleanup, even when attach itself fails. */
    session->attach_attempted = 1;
    int r = ops->attach(&session->record, mac, name);
    if (r == 0) session->attached = 1;
    return r;
}

int vnet_close(vnet_session_t *session, const vnet_ops_t *ops)
{
    if (session->attach_attempted) {
        int r = ops->detach(&session->record);
        if (r != 0) return r;
        session->attach_attempted = session->attached = 0;
    }
    if (session->created) {
        int r = ops->destroy(&session->record);
        if (r != 0) return r;
        session->created = 0;
        memset(&session->record, 0, sizeof(session->record));
    }
    return 0;
}

void vnet_forget_after_netterm(vnet_session_t *session)
{
    memset(session, 0, sizeof(*session));
}

int vnet_complete_address_ioctl(vnet_record_t *expected, vnet_record_t *actual,
    unsigned command, int (*complete)(vnet_record_t *, unsigned, int))
{
    if (actual != expected || (command != 2 && command != 3)) return -1;
    /* Local Ethernet fixture has no hardware address-programming work.
     * Acknowledge precisely once, including the native waiter notification. */
    return complete(actual, command, 0);
}
