#ifndef PSP_USB_ROUTER_FILTER_H
#define PSP_USB_ROUTER_FILTER_H
#include "lab_gateway.h"
#include <string.h>
/* Restrict the USB-facing LAN to its configured PSP source. lwIP still owns
 * protocol validation/checksums and forwarding decisions. */
static inline int usb_router_frame_allowed(const uint8_t *f,unsigned n)
{
    if(n<14 || n>VNET_ETH_MAX || memcmp(f+6,vnet_lab_psp_mac,6))return 0;
    unsigned type=(unsigned)f[12]<<8|f[13];
    if(type==0x0800) {
        if(n<34 || (f[14]>>4)!=4)return 0;
        unsigned header=(f[14]&15)*4,total=(unsigned)f[16]<<8|f[17];
        return header>=20 && total>=header && total<=n-14 && !memcmp(f+26,vnet_lab_psp_ip,4);
    }
    if(type==0x0806) {
        return n>=42 && f[14]==0 && f[15]==1 && f[16]==8 && f[17]==0
            && f[18]==6 && f[19]==4 && f[20]==0 && (f[21]==1 || f[21]==2)
            && !memcmp(f+22,vnet_lab_psp_mac,6) && !memcmp(f+28,vnet_lab_psp_ip,4);
    }
    return 0;
}
#endif
