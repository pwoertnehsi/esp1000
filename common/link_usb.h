#ifndef PSP_LINK_USB_DESCRIPTOR_H
#define PSP_LINK_USB_DESCRIPTOR_H

#include <stdint.h>
#include "link_protocol.h"

/* Validate a single interface and discover its two full-speed bulk endpoints.
 * Do not assume addresses: the PSP bus driver assigns composite endpoints. */
static inline int link_usb_endpoints(const uint8_t *data, unsigned size,
                                     uint8_t *in, uint8_t *out)
{
    unsigned pos = 9;
    unsigned found = 0;
    uint8_t in_addr = 0, out_addr = 0;
    if (size < 9 || data[0] != 9 || data[1] != 4 || data[3] != 0 ||
        data[4] != 2 || data[5] != 0xff || data[6] != LINK_SUBCLASS ||
        data[7] != LINK_PROTOCOL) return 0;
    while (pos + 2 <= size && data[pos + 1] != 4) {
        unsigned n = data[pos];
        if (n < 2 || n > size - pos) return 0;
        if (data[pos + 1] == 5) {
            if (n != 7 || (data[pos + 3] & 3) != 2 ||
                data[pos + 4] != 64 || data[pos + 5] != 0 ||
                !(data[pos + 2] & 15) || (data[pos + 2] & 0x70)) return 0;
            if (data[pos + 2] & 0x80) {
                if (in_addr) return 0;
                in_addr = data[pos + 2];
            } else {
                if (out_addr) return 0;
                out_addr = data[pos + 2];
            }
            found++;
        }
        pos += n;
    }
    if (found != 2 || !in_addr || !out_addr) return 0;
    *in = in_addr;
    *out = out_addr;
    return pos;
}

#endif
