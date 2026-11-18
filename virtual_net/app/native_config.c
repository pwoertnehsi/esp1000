#include <pspnet_inet.h>
#include <netinet/in.h>
#include <string.h>
#include "native_config.h"
#include "lab_gateway.h"

int vnet_configure_lab(int (*report)(const char *, int))
{
    vnet_ioctl_fn ioctl = vnet_resolve_ioctl();
    if (!ioctl) { report("CONFIG_IOCTL_ABI_REFUSED", -1); return -1; }
    /* Exact byte layout used by the supplied 6.61 alias wrapper. This avoids
     * relying on the host/PSP difference in sockaddr length/family packing. */
    unsigned char request[64] = {0};
    memcpy(request, "usb", 4);
    request[16] = request[32] = request[48] = 16;
    request[17] = request[33] = AF_INET;
    memcpy(request + 20, vnet_lab_psp_ip, 4);
    static const unsigned char broadcast[4] = {192, 168, 77, 255};
    static const unsigned char mask[4] = {255, 255, 255, 0};
    memcpy(request + 36, broadcast, 4);
    memcpy(request + 52, mask, 4);
    if (report("CONFIG_SOCKET_BEGIN", 0) < 0) return -1;
    int socket = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
    int result = -1;
    if (report("CONFIG_SOCKET_END", socket) < 0 || socket < 0) goto close;
    if (report("CONFIG_ALIAS_BEGIN", 0) < 0) goto close;
    int r = ioctl(socket, 0x8040691au, request);
    if (report("CONFIG_ALIAS_END", r) >= 0 && r == 0) result = 0;
close:
    if (socket >= 0) {
        int r = sceNetInetClose(socket);
        report("CONFIG_SOCKET_CLOSE", r);
        if (r != 0) result = -1;
    }
    return result;
}
