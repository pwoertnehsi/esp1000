#ifndef PSP_ESP_LINK_PROTOCOL_H
#define PSP_ESP_LINK_PROTOCOL_H

#include <stdint.h>
#include <string.h>

#define LINK_VID 0x054c
#define LINK_PID 0x0282
#define LINK_SUBCLASS 0x57
#define LINK_PROTOCOL 1
#define LINK_FRAME_SIZE 64
#define LINK_PAYLOAD_SIZE 48
#define LINK_PING 1
#define LINK_PONG 2
#define LINK_TEXT 3
#define LINK_HELLO 4
#define LINK_FILE_BEGIN 5
#define LINK_FILE_DATA 6
#define LINK_FILE_END 7
#define LINK_FILE_ACK 8
#define LINK_FILE_ERROR 9
#define LINK_GET_PAGE 10
#define LINK_FILE_ABORT 11
#define LINK_PACKET 12
#define LINK_NET_STATUS 13
#define LINK_WIFI_INFO 14
#define LINK_WIFI_SCAN_REQUEST 15
#define LINK_WIFI_SCAN_RESULT 16
#define LINK_WIFI_PROFILE 17
#define LINK_WIFI_PROFILE_RESULT 18
#define LINK_LAST_TYPE LINK_WIFI_PROFILE_RESULT
#define LINK_TEST_HOST "neverssl.com"
/* A pending IN receive is allowed to remain idle indefinitely. Only outgoing
 * transfers represent promised progress and are subject to the watchdog. */
static inline int link_tx_stalled(int busy, uint64_t elapsed_ms, unsigned timeout_ms)
{ return busy && elapsed_ms > timeout_ms; }
#define LINK_HTML_HELLO "PSP HTML receiver v5"
#define LINK_FILE_LIMIT (128u * 1024u)
#define LINK_FILE_CHUNK 44

/* Exactly one 64-byte USB transfer per frame, little endian on both CPUs.
 * seq identifies the sender's packet; reply_to identifies an echoed packet. */
typedef struct {
    uint8_t magic[4];
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t seq;
    uint32_t reply_to;
    uint8_t payload[LINK_PAYLOAD_SIZE];
} link_frame_t;

_Static_assert(sizeof(link_frame_t) == LINK_FRAME_SIZE, "USB frame layout");

static inline int link_valid(const link_frame_t *f, unsigned size)
{
    return size == LINK_FRAME_SIZE && memcmp(f->magic, "PSPW", 4) == 0 &&
        f->version == 1 && f->length <= LINK_PAYLOAD_SIZE &&
        f->type >= LINK_PING && f->type <= LINK_LAST_TYPE;
}

static inline uint32_t link_get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void link_put_u32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) p[i] = value >> (8 * i);
}

static inline uint32_t link_crc_update(uint32_t state, const void *data, unsigned length)
{
    const uint8_t *p = data;
    while (length--) {
        state ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            state = (state >> 1) ^ (0xedb88320u & (0u - (state & 1u)));
    }
    return state;
}

static inline void link_init(link_frame_t *f, uint8_t type, uint32_t seq,
                             uint32_t reply_to, const char *text)
{
    unsigned n = 0;
    memset(f, 0, sizeof(*f));
    memcpy(f->magic, "PSPW", 4);
    f->version = 1;
    f->type = type;
    f->seq = seq;
    f->reply_to = reply_to;
    if (text) {
        while (n < LINK_PAYLOAD_SIZE && text[n]) n++;
        f->length = n;
        memcpy(f->payload, text, n);
    }
}

#endif
