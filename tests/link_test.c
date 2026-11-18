#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "link_protocol.h"
#include "link_usb.h"

static void test_frames(void)
{
    assert(!link_tx_stalled(0, 60000, 5000)); /* idle receive is not a stuck send */
    assert(!link_tx_stalled(1, 5000, 5000));
    assert(link_tx_stalled(1, 5001, 5000));
    link_frame_t frame;
    char long_text[100];
    memset(long_text, 'x', sizeof(long_text));
    link_init(&frame, LINK_PING, 0x12345678, 0, "hello");
    assert(link_valid(&frame, sizeof(frame)));
    assert(offsetof(link_frame_t, payload) == 16);
    assert(frame.length == 5 && !memcmp(frame.payload, "hello", 5));
    const uint8_t *wire = (const uint8_t *)&frame;
    assert(wire[8] == 0x78 && wire[11] == 0x12); /* Both targets are LE. */
    assert(!link_valid(&frame, 63));
    frame.magic[0] = 'X';
    assert(!link_valid(&frame, 64));
    link_init(&frame, LINK_TEXT, UINT32_MAX, 7, long_text);
    assert(frame.length == 48 && frame.reply_to == 7);
    assert(link_valid(&frame, 64));
    frame.length = 49;
    assert(!link_valid(&frame, 64));
    frame.length = 0;
    frame.version = 2;
    assert(!link_valid(&frame, 64));
    frame.version = 1;
    frame.type = 0;
    assert(!link_valid(&frame, 64));
    frame.type = LINK_LAST_TYPE + 1;
    assert(!link_valid(&frame, 64));
    link_init(&frame, LINK_PONG, 1, UINT32_MAX, NULL);
    assert(link_valid(&frame, 64) && frame.reply_to == UINT32_MAX);
}

static void test_descriptors(void)
{
    /* Deliberately use reassigned endpoints and interface number 1. */
    uint8_t descriptor[] = {9,4,1,0,2,0xff,LINK_SUBCLASS,LINK_PROTOCOL,0,
                            7,5,0x82,2,64,0,0, 7,5,0x03,2,64,0,0};
    uint8_t in = 0, out = 0;
    assert(link_usb_endpoints(descriptor, sizeof(descriptor), &in, &out) == 23);
    assert(in == 0x82 && out == 0x03);
    for (unsigned n = 0; n < sizeof(descriptor); n++)
        assert(!link_usb_endpoints(descriptor, n, &in, &out));
    descriptor[6] = 0; /* Sony accessory interface must not match. */
    assert(!link_usb_endpoints(descriptor, sizeof(descriptor), &in, &out));
    descriptor[6] = LINK_SUBCLASS;
    descriptor[12] = 3; /* Interrupt endpoint. */
    assert(!link_usb_endpoints(descriptor, sizeof(descriptor), &in, &out));
    descriptor[12] = 2;
    descriptor[13] = 0; descriptor[14] = 2; /* 512-byte high-speed packet. */
    assert(!link_usb_endpoints(descriptor, sizeof(descriptor), &in, &out));
    descriptor[13] = 64; descriptor[14] = 0;
    descriptor[18] = 0x83; /* Both endpoints IN. */
    assert(!link_usb_endpoints(descriptor, sizeof(descriptor), &in, &out));
    descriptor[18] = 0x03;
    descriptor[9] = 0; /* Must not loop on zero-length descriptor. */
    assert(!link_usb_endpoints(descriptor, sizeof(descriptor), &in, &out));
    /* Exercise malformed/truncated descriptor handling under ASan/UBSan. */
    uint32_t random = 1;
    uint8_t bytes[64];
    for (unsigned i = 0; i < 100000; i++) {
        for (unsigned j = 0; j < sizeof(bytes); j++) {
            random = random * 1664525u + 1013904223u;
            bytes[j] = random >> 24;
        }
        if (i & 1) memcpy(bytes, (uint8_t[]){9,4,1,0,2,0xff,LINK_SUBCLASS,LINK_PROTOCOL,0}, 9);
        link_usb_endpoints(bytes, i % 65, &in, &out);
    }
}

int main(void)
{
    test_frames();
    test_descriptors();
    puts("PASS: wire format, bounds, endpoint discovery, 100000 malformed descriptors");
    return 0;
}
