#ifndef PSP_NATIVE_ROUTE_H
#define PSP_NATIVE_ROUTE_H
#include "link_protocol.h"
/* 6.61 BSD routing socket ABI recovered from Sony's INET route wrappers:
 * 76-byte rt_msghdr, version3, RTM_ADD1, RTF_UP|RTF_GATEWAY, 3 sockaddrs.
 * Byte construction avoids host/PSP sockaddr and native-long size differences. */
#define VNET_ROUTE_SIZE 124u
#define VNET_ROUTE_SEQ 0x57504601u
static inline void vnet_default_route(uint8_t out[VNET_ROUTE_SIZE])
{
    memset(out,0,VNET_ROUTE_SIZE);
    out[0]=VNET_ROUTE_SIZE; out[2]=3; out[3]=1;
    link_put_u32(out+8,3); link_put_u32(out+12,7); link_put_u32(out+20,VNET_ROUTE_SEQ);
    out[76]=out[92]=out[108]=16;
    out[77]=out[93]=2;
    out[96]=192;out[97]=168;out[98]=77;out[99]=1;
}
#endif
