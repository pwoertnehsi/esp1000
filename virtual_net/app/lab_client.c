#include <pspnet_inet.h>
#include <netinet/in.h>
#include <string.h>
#include "native_config.h"
#include "lab_client.h"
#include "lab_gateway.h"

extern int sceNetConfigGetEtherAddr(const char *name, unsigned char *mac);

int vnet_run_lab_client(int (*report)(const char *, int))
{
    unsigned char payload[1472], received[1472];
    /* Ethernet sizes: 68, 64, 65, 208, 209, 512, 1500, 1514 bytes. */
    static const unsigned sizes[] = {26, 22, 23, 166, 167, 470, 1458, 1472};
    for (unsigned i = 0; i < sizeof(payload); ++i) payload[i] = (unsigned char)(i * 37u + 3);
    payload[0] = 0; payload[1] = 255;
    unsigned char mac[8] = {0};
    int socket = -1, result = -1;
    if (report("LAB_MAC_BEGIN", 0) < 0) return -1;
    int r = sceNetConfigGetEtherAddr("usb", mac);
    if (report("LAB_MAC_END", r) < 0 || r != 0 || memcmp(mac, vnet_lab_psp_mac, 6)) return -1;
    if (report("LAB_CONFIG_BEGIN", 0) < 0) return -1;
    r = vnet_configure_lab(report);
    if (report("LAB_CONFIG_END", r) < 0 || r != 0) return -1;
    struct sockaddr_in local, gateway, source;
    memset(&local, 0, sizeof(local));
    memset(&gateway, 0, sizeof(gateway));
    memset(&source, 0, sizeof(source));
#ifdef __PSP__
    local.sin_len = sizeof(local); gateway.sin_len = sizeof(gateway);
#endif
    local.sin_family = gateway.sin_family = AF_INET;
    memcpy(&local.sin_addr.s_addr, vnet_lab_psp_ip, 4);
    memcpy(&gateway.sin_addr.s_addr, vnet_lab_gateway_ip, 4);
    gateway.sin_port = htons(VNET_LAB_PORT);
    socket = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
    if (report("LAB_SOCKET", socket) < 0 || socket < 0) goto cleanup;
    r = sceNetInetBind(socket, (struct sockaddr *)&local, sizeof(local));
    if (report("LAB_BIND", r) < 0 || r < 0) goto cleanup;
    for (unsigned trial = 0; trial < sizeof(sizes) / sizeof(sizes[0]); ++trial) {
        unsigned bytes = sizes[trial];
        payload[2] = (unsigned char)trial;
        if (report("LAB_UDP_SIZE", (int)bytes) < 0) goto cleanup;
        r = (int)sceNetInetSendto(socket, payload, bytes, MSG_DONTWAIT,
                                 (struct sockaddr *)&gateway, sizeof(gateway));
        if (report("LAB_UDP_SEND", r) < 0 || r != (int)bytes) goto cleanup;
        struct SceNetInetPollfd pollfd = {socket, SCE_NET_INET_POLLIN, 0};
        r = sceNetInetPoll(&pollfd, 1, 3000);
        if (report("LAB_UDP_POLL", r) < 0 || r != 1
            || !(pollfd.revents & SCE_NET_INET_POLLIN)
            || (pollfd.revents & (SCE_NET_INET_POLLERR | SCE_NET_INET_POLLHUP | SCE_NET_INET_POLLNVAL))) goto cleanup;
        socklen_t length = sizeof(source);
        r = (int)sceNetInetRecvfrom(socket, received, sizeof(received), MSG_DONTWAIT,
                                  (struct sockaddr *)&source, &length);
        if (report("LAB_UDP_RECEIVE", r) < 0 || r != (int)bytes
            || length != sizeof(source) || source.sin_family != AF_INET
            || source.sin_addr.s_addr != gateway.sin_addr.s_addr
            || source.sin_port != gateway.sin_port || memcmp(received, payload, bytes)) goto cleanup;
    }
    result = 0;
cleanup:
    if (socket >= 0) {
        r = sceNetInetClose(socket);
        report("LAB_SOCKET_CLOSE", r);
        if (r != 0) result = -1;
    }
    if (report("LAB_CLIENT_RESULT", result) < 0) result = -1;
    return result;
}
