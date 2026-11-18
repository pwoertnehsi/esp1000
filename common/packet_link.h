#ifndef PSP_PACKET_LINK_H
#define PSP_PACKET_LINK_H
#include "link_protocol.h"
#define LINK_PACKET_HELLO "PSP Ethernet lab v1"
#define LINK_NET_HELLO "PSP Ethernet net v1"
#define LINK_BROWSER_HELLO "PSP Ethernet net v2"
#define LINK_NATIVE_HELLO "PSP native WiFi v1"
#define PACKET_MAX 1514u
#define PACKET_CHUNK 36u
/* One bounded packet in each direction; USB guarantees ordered delivery.
 * Header: packet id, total length, byte offset, whole-packet CRC32 (u32 LE).
 * No native addresses cross this boundary. A new offset-zero record replaces
 * unfinished state. The owner resets both states on disconnect/USB errors. */
typedef struct {
    uint8_t data[PACKET_MAX];
    uint32_t id, total, offset, crc;
} packet_link_t;
static inline int packet_link_begin(packet_link_t *s, const void *data, unsigned n, uint32_t id)
{
    if (s->total || n < 14 || n > PACKET_MAX || !id) return 0;
    memcpy(s->data, data, n); s->id=id; s->total=n; s->offset=0;
    s->crc=link_crc_update(~0u,data,n)^~0u;
    return 1;
}
static inline int packet_link_next(packet_link_t *s, link_frame_t *f, uint32_t seq)
{
    if (!s->total) return 0;
    unsigned n=s->total-s->offset;
    if (n>PACKET_CHUNK) n=PACKET_CHUNK;
    link_init(f,LINK_PACKET,seq,0,NULL);
    /* total and offset are u16: whole header fits 12 bytes. */
    link_put_u32(f->payload,s->id);
    f->payload[4]=s->total; f->payload[5]=s->total>>8;
    f->payload[6]=s->offset; f->payload[7]=s->offset>>8;
    link_put_u32(f->payload+8,s->crc);
    memcpy(f->payload+12,s->data+s->offset,n); f->length=12+n;
    s->offset+=n;
    if (s->offset==s->total) s->total=0;
    return 1;
}
/* -1 malformed/reset; 0 incomplete; positive complete byte count. */
static inline int packet_link_receive(packet_link_t *s, const link_frame_t *f)
{
    if (!link_valid(f,sizeof(*f)) || f->type!=LINK_PACKET || f->length<=12) goto bad;
    unsigned total=f->payload[4]|(unsigned)f->payload[5]<<8;
    unsigned offset=f->payload[6]|(unsigned)f->payload[7]<<8;
    unsigned n=f->length-12;
    uint32_t id=link_get_u32(f->payload),crc=link_get_u32(f->payload+8);
    if (!id || total<14 || total>PACKET_MAX || offset>=total || n>total-offset
        || n!=(total-offset>PACKET_CHUNK?PACKET_CHUNK:total-offset)) goto bad;
    if (!offset) { s->id=id; s->total=total; s->offset=0; s->crc=crc; }
    if (s->id!=id || s->total!=total || s->offset!=offset || s->crc!=crc) goto bad;
    memcpy(s->data+offset,f->payload+12,n); s->offset+=n;
    if (s->offset!=total) return 0;
    s->total=0;
    if ((link_crc_update(~0u,s->data,total)^~0u)!=crc) goto bad;
    return total;
bad:
    s->total=s->offset=0;
    return -1;
}
#endif
