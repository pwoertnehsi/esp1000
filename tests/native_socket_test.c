#include "native_socket_shim.h"
#include "../common/native_check.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
static int scenario, net_inits, inet_inits, net_terms, inet_terms, closed, sockets, reports;
static unsigned char queued[128];
static size_t queued_size;
static struct sockaddr_in bound;
static int report(const char *s, int r)
{ (void)s; (void)r; reports++; return scenario==10 ? -1 : 0; }
int sceNetInit(int p,int a,int b,int c,int d)
{ assert(p>=32768 && a>0 && b>0 && c>0 && d>0); net_inits++; return scenario==1 ? -1 : 0; }
int sceNetTerm(void) { net_terms++; return scenario==9 ? -1 : 0; }
int sceNetInetInit(void) { inet_inits++; return scenario==2 ? -1 : 0; }
int sceNetInetTerm(void) { inet_terms++; return scenario==8 ? -1 : 0; }
int sceNetInetSocket(int d,int t,int p)
{ if (scenario==3) return -1; assert(d==AF_INET && t==SOCK_DGRAM && p==0); sockets++; return 17; }
int sceNetInetBind(int s,const struct sockaddr *a,socklen_t l)
{ assert(s==17 && l==sizeof(bound)); memcpy(&bound,a,l); bound.sin_port=htons(49152); return scenario==4 ? -1 : 0; }
int sceNetInetGetsockname(int s,struct sockaddr *a,socklen_t *l)
{
    assert(s==17 && *l==sizeof(bound)); memcpy(a,&bound,sizeof(bound));
    int r=0;
    if (scenario==5) ((struct sockaddr_in *)a)->sin_addr.s_addr=0;
    return r;
}
size_t sceNetInetSendto(int s,const void *b,size_t n,int f,const struct sockaddr *a,socklen_t l)
{
    assert(s==17 && f==MSG_DONTWAIT && l==sizeof(bound) && n<=sizeof(queued));
    assert(!memcmp(a,&bound,l));
    memcpy(queued,b,n); queued_size=n;
    return scenario==11 ? (size_t)-1 : n;
}
int sceNetInetPoll(struct SceNetInetPollfd *p,size_t n,int t)
{
    assert(n==1 && t==1000);
    if (scenario==6) return 0;
    assert(p->fd==17 && p->events==SCE_NET_INET_POLLIN && queued_size);
    p->revents=SCE_NET_INET_POLLIN; return 1;
}
size_t sceNetInetRecvfrom(int s,void *b,size_t n,int f,struct sockaddr *a,socklen_t *l)
{
    if (scenario==12) return (size_t)-1;
    assert(s==17 && f==MSG_DONTWAIT && *l==sizeof(bound) && n>=queued_size);
    memcpy(b,queued,queued_size); memcpy(a,&bound,*l);
    ssize_t r=(ssize_t)queued_size;
    if (scenario==7 && r>0) ((char *)b)[0]^=1;
    return (size_t)r;
}
int sceNetInetClose(int s)
{ assert(s==17); closed++; return scenario==13 ? -1 : 0; }
int main(int argc,char **argv)
{
    assert(argc==2); scenario=atoi(argv[1]);
    int r=native_socket_check(report);
    assert(reports>0 && closed==sockets);
    if (scenario==0) assert(r==0);
    else if (scenario==8 || scenario==9) assert(r==-2);
    else assert(r==-1);
    assert(net_inits==(scenario==10 ? 0 : 1));
    assert(inet_inits==(scenario==10 || scenario==1 ? 0 : 1));
    assert(net_terms==(scenario==10 || scenario==1 || scenario==8 ? 0 : 1));
    assert(inet_terms==(scenario==10 || scenario==1 || scenario==2 ? 0 : 1));
    return 0;
}
