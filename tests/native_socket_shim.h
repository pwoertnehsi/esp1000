#ifndef NATIVE_SOCKET_SHIM_H
#define NATIVE_SOCKET_SHIM_H
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#define SCE_NET_INET_POLLOUT POLLOUT
#define SCE_NET_INET_POLLIN POLLIN
#define SCE_NET_INET_POLLERR POLLERR
#define SCE_NET_INET_POLLHUP POLLHUP
#define SCE_NET_INET_POLLNVAL POLLNVAL
struct SceNetInetPollfd { int fd; short events, revents; };
int sceNetInit(int,int,int,int,int);
int sceNetTerm(void);
int sceNetInetInit(void);
int sceNetInetTerm(void);
int sceNetInetSocket(int,int,int);
int sceNetInetBind(int,const struct sockaddr *,socklen_t);
int sceNetInetGetsockname(int,struct sockaddr *,socklen_t *);
size_t sceNetInetSendto(int,const void *,size_t,int,const struct sockaddr *,socklen_t);
int sceNetInetPoll(struct SceNetInetPollfd *,size_t,int);
size_t sceNetInetRecvfrom(int,void *,size_t,int,struct sockaddr *,socklen_t *);
int sceNetInetClose(int);
#endif
