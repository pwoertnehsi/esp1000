#include "native_socket_shim.h"
#include "../virtual_net/app/lab_client.h"
#include "lab_gateway.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
static int scenario,configs,opened,closed,sends,receives;
static unsigned char payload[1600], previous[1600];
static size_t previous_size;
static size_t payload_size;
static int report(const char *name,int r)
{ (void)r;return scenario==12 && !strcmp(name,"LAB_CONFIG_BEGIN") ? -1 : 0; }
int sceNetConfigGetEtherAddr(const char *name,unsigned char *mac)
{ assert(!strcmp(name,"usb"));memcpy(mac,vnet_lab_psp_mac,6);if(scenario==2)mac[0]^=1;return scenario==1 ? -1 : 0; }
int vnet_configure_lab(int (*fn)(const char *,int)) { (void)fn;configs++;return scenario==3 ? -1 : 0; }
int sceNetInetSocket(int d,int t,int p)
{ assert(configs && d==AF_INET && t==SOCK_DGRAM && p==0);if(scenario==4)return -1;opened++;return 17; }
int sceNetInetBind(int s,const struct sockaddr *a,socklen_t l)
{
    const struct sockaddr_in *in=(const void *)a;
    assert(s==17 && l==sizeof(*in) && in->sin_family==AF_INET && !in->sin_port);
    assert(!memcmp(&in->sin_addr.s_addr,vnet_lab_psp_ip,4));return scenario==5 ? -1 : 0;
}
size_t sceNetInetSendto(int s,const void *b,size_t n,int f,const struct sockaddr *a,socklen_t l)
{
    const struct sockaddr_in *in=(const void *)a;
    assert(s==17 && f==MSG_DONTWAIT && l==sizeof(*in) && in->sin_port==htons(VNET_LAB_PORT));
    assert(!memcmp(&in->sin_addr.s_addr,vnet_lab_gateway_ip,4) && n<sizeof(payload));
    assert(memchr(b,0,n) && memchr(b,255,n));memcpy(previous,payload,payload_size);previous_size=payload_size;
    memcpy(payload,b,n);payload_size=n;sends++;
    return scenario==6 ? (size_t)-1 : n;
}
int sceNetInetPoll(struct SceNetInetPollfd *p,size_t n,int timeout)
{ assert(n==1 && p->fd==17 && timeout==3000);p->revents=SCE_NET_INET_POLLIN;return scenario==7 ? 0 : 1; }
size_t sceNetInetRecvfrom(int s,void *b,size_t n,int f,struct sockaddr *a,socklen_t *l)
{
    assert(s==17 && f==MSG_DONTWAIT && n>=payload_size);
    if(scenario==16)return (size_t)-1;
    struct sockaddr_in *in=(void *)a;memset(in,0,sizeof(*in));
    in->sin_family=scenario==14 ? AF_UNSPEC : AF_INET;in->sin_port=htons(VNET_LAB_PORT);
    memcpy(&in->sin_addr.s_addr,vnet_lab_gateway_ip,4);
    if(scenario==8)in->sin_addr.s_addr=0;
    if(scenario==9)in->sin_port=0;
    if(scenario==15)*l=0;
    receives++;
    if(scenario==13 && sends>1){memcpy(b,previous,previous_size);return previous_size;}
    memcpy(b,payload,payload_size);if(scenario==10)((unsigned char *)b)[0]^=1;
    return payload_size;
}
int sceNetInetClose(int s) { assert(s==17);closed++;return scenario==11 ? -1 : 0; }
int main(int argc,char **argv)
{
    assert(argc==2);scenario=atoi(argv[1]);int r=vnet_run_lab_client(report);
    assert(r==(scenario==0 ? 0 : -1));assert(opened==closed);if(scenario==0)assert(sends==8 && receives==8);
    if(scenario==1 || scenario==2 || scenario==12)assert(!configs && !opened);
    return 0;
}
