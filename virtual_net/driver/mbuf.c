#include "mbuf.h"
#include <string.h>

/* Native little-endian MIPS layout, independent of host pointer size. */
typedef struct {
    uint32_t next, next_packet, data;
    int32_t length;
    uint16_t type, flags;
} mbuf_header_t;
_Static_assert(sizeof(mbuf_header_t) == 20, "native mbuf header");

int vnet_mbuf_copy(const vnet_mbuf_ops_t *ops, uint32_t head,
                    uint8_t *out, size_t capacity, size_t *length)
{
    size_t used = 0;
    uint32_t packet_length = 0;
    int has_packet_header = 0;
    *length = 0;
    for (unsigned count = 0; head && count < 32; ++count) {
        mbuf_header_t header;
        if (!ops->read(ops->context, head, &header, sizeof(header))
            || header.length < 0 || (unsigned)header.length > VNET_MBUF_MAX
            || (unsigned)header.length > capacity - used
            || (unsigned)header.length > VNET_MBUF_MAX - used) return 0;
        if (count == 0 && (header.flags & 2)) {
            has_packet_header = 1;
            if (head > UINT32_MAX - 24
                || !ops->read(ops->context, head + 24, &packet_length, 4)
                || packet_length > VNET_MBUF_MAX || packet_length < 14) return 0;
        }
        if (header.length && !ops->read(ops->context, header.data, out + used,
                                       (size_t)header.length)) return 0;
        used += (size_t)header.length;
        head = header.next;
    }
    if (head || used < 14 || (has_packet_header && used != packet_length)) return 0;
    *length = used;
    return 1;
}

uint32_t vnet_mbuf_make(const vnet_mbuf_ops_t *ops, const uint8_t *data,
                        size_t length, uint32_t interface)
{
    if (length < 14 || length > VNET_MBUF_MAX) return 0;
    uint32_t head = 0, previous = 0;
    size_t offset = 0;
    while (offset < length) {
        uint32_t node = ops->get(ops->context);
        if (!node) goto fail;
        /* Sony WLAN RX at 4438..449c reserves two extra bytes before the
         * Ethernet header. After removing 14 bytes, IPv4/TCP headers must be
         * word aligned on Allegrex. UDP-only checks did not catch this. */
        unsigned data_offset = head ? 20 : 50;
        if (node > UINT32_MAX - 256) { ops->free_chain(ops->context, node); goto fail; }
        size_t take = length - offset;
        if (take > 256u - data_offset) take = 256u - data_offset;
        mbuf_header_t header = {0, 0, node + data_offset, (int32_t)take, 1, head ? 0 : 2};
        uint32_t packet_header[7] = {interface, (uint32_t)length, 0, 0, 0, 0, 0};
        if (!ops->write(ops->context, node, &header, sizeof(header))
            || (!head && !ops->write(ops->context, node + 20, packet_header, sizeof(packet_header)))
            || !ops->write(ops->context, node + data_offset, data + offset, take)
            || (previous && !ops->write(ops->context, previous, &node, sizeof(node)))) {
            ops->free_chain(ops->context, node);
            goto fail;
        }
        if (!head) head = node;
        previous = node;
        offset += take;
    }
    return head;
fail:
    if (head) ops->free_chain(ops->context, head);
    return 0;
}
