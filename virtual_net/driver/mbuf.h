#ifndef PSP_VNET_MBUF_H
#define PSP_VNET_MBUF_H
#include <stddef.h>
#include <stdint.h>
#define VNET_MBUF_MAX 1514u
/* Read/write operate on PSP 32-bit pool addresses. Host tests use a bounded
 * pool model; writes are all-or-nothing; kernel implementation receives only native allocator-owned data. */
typedef struct {
    void *context;
    int (*read)(void *, uint32_t, void *, size_t);
    int (*write)(void *, uint32_t, const void *, size_t);
    uint32_t (*get)(void *);
    void (*free_chain)(void *, uint32_t);
} vnet_mbuf_ops_t;
int vnet_mbuf_copy(const vnet_mbuf_ops_t *, uint32_t head,
                    uint8_t *out, size_t capacity, size_t *length);
uint32_t vnet_mbuf_make(const vnet_mbuf_ops_t *, const uint8_t *data,
                        size_t length, uint32_t interface);
#endif
