#ifndef VNET_FRAME_QUEUE_H
#define VNET_FRAME_QUEUE_H
#include <stdint.h>
#include <string.h>
#define VNET_FRAME_SLOTS 8
#define VNET_FRAME_CAPACITY 1514
/* Owners provide the interrupt lock. Buffers contain bytes, never native mbufs. */
typedef struct {unsigned read,write;struct {unsigned size;uint8_t data[VNET_FRAME_CAPACITY];} rows[VNET_FRAME_SLOTS];} vnet_frame_queue_t;
static inline int vnet_frame_space(const vnet_frame_queue_t *q){return q->write-q->read<VNET_FRAME_SLOTS;}
static inline int vnet_frame_push(vnet_frame_queue_t *q,const void *data,unsigned size)
{
    if(size<14 || size>VNET_FRAME_CAPACITY || !vnet_frame_space(q))return 0;
    unsigned slot=q->write%VNET_FRAME_SLOTS;memcpy(q->rows[slot].data,data,size);q->rows[slot].size=size;++q->write;return 1;
}
static inline unsigned vnet_frame_pop(vnet_frame_queue_t *q,void *data)
{
    if(q->read==q->write)return 0;
    unsigned slot=q->read%VNET_FRAME_SLOTS,size=q->rows[slot].size;
    memcpy(data,q->rows[slot].data,size);++q->read;return size;
}
#endif
