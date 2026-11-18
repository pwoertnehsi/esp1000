#include "native_check.h"
#include <pspnet.h>
#include <pspnet_inet.h>
#include <netinet/in.h>
#include <string.h>

int native_socket_check(int (*report)(const char *, int))
{
    static const unsigned char payload[] = "PSP native UDP\0loopback\xff";
    unsigned char received[sizeof(payload)];
    int net_owned = 0, inet_owned = 0, socket = -1, result = -1;
    int r;
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    memset(&address, 0, sizeof(address));
#ifdef __PSP__
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (report("NET_INIT_BEGIN", 0) < 0) return -1;
    r = sceNetInit(128 * 1024, 42, 4096, 42, 4096);
    net_owned = r == 0;
    if (report("NET_INIT_END", r) < 0 || r != 0) goto cleanup;
    if (report("INET_INIT_BEGIN", 0) < 0) goto cleanup;
    r = sceNetInetInit();
    inet_owned = r == 0;
    if (report("INET_INIT_END", r) < 0 || r != 0) goto cleanup;
    socket = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
    if (report("UDP_SOCKET", socket) < 0 || socket < 0) goto cleanup;
    r = sceNetInetBind(socket, (struct sockaddr *)&address, sizeof(address));
    if (report("UDP_BIND", r) < 0 || r < 0) goto cleanup;
    r = sceNetInetGetsockname(socket, (struct sockaddr *)&address, &length);
    if (report("UDP_GETSOCKNAME", r) < 0 || r < 0 || length != sizeof(address)
        || address.sin_family != AF_INET || address.sin_port == 0
        || address.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) goto cleanup;
    r = (int)sceNetInetSendto(socket, payload, sizeof(payload), MSG_DONTWAIT,
                            (struct sockaddr *)&address, sizeof(address));
    if (report("UDP_SEND", r) < 0 || r != sizeof(payload)) goto cleanup;
    struct SceNetInetPollfd pollfd = { socket, SCE_NET_INET_POLLIN, 0 };
    r = sceNetInetPoll(&pollfd, 1, 1000);
    if (report("UDP_POLL", r) < 0 || r != 1
        || !(pollfd.revents & SCE_NET_INET_POLLIN)
        || (pollfd.revents & (SCE_NET_INET_POLLERR | SCE_NET_INET_POLLHUP
                              | SCE_NET_INET_POLLNVAL))) goto cleanup;
    struct sockaddr_in source;
    socklen_t source_length = sizeof(source);
    memset(&source, 0, sizeof(source));
    r = (int)sceNetInetRecvfrom(socket, received, sizeof(received), MSG_DONTWAIT,
                              (struct sockaddr *)&source, &source_length);
    if (report("UDP_RECEIVE", r) < 0 || r != sizeof(payload)
        || source_length != sizeof(source) || source.sin_family != AF_INET
        || source.sin_port != address.sin_port
        || source.sin_addr.s_addr != address.sin_addr.s_addr
        || memcmp(received, payload, sizeof(payload))) goto cleanup;
    result = 0;
cleanup:
    if (socket >= 0) {
        r = sceNetInetClose(socket);
        report("UDP_CLOSE", r);
        if (r < 0) result = -1;
    }
    if (inet_owned) {
        report("INET_TERM_BEGIN", 0);
        r = sceNetInetTerm();
        report("INET_TERM_END", r);
        if (r != 0) return -2;
    }
    if (net_owned) {
        report("NET_TERM_BEGIN", 0);
        r = sceNetTerm();
        report("NET_TERM_END", r);
        if (r != 0) return -2;
    }
    if (report("UDP_LOOPBACK_RESULT", result) < 0) result = -1;
    return result;
}
