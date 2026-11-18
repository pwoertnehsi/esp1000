#include "native_socket_shim.h"
#include "../virtual_net/app/native_config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
static int scenario,opened,closed,ioctls,begin,end,socket_begin,socket_end;
static int report(const char *name,int result)
{
    (void)result;
    if(!strcmp(name,"CONFIG_ALIAS_BEGIN"))begin++;
    if(!strcmp(name,"CONFIG_ALIAS_END"))end++;
    if(!strcmp(name,"CONFIG_SOCKET_BEGIN"))socket_begin++;
    if(!strcmp(name,"CONFIG_SOCKET_END"))socket_end++;
    if(scenario==4 && !strcmp(name,"CONFIG_ALIAS_BEGIN"))return -1;
    if(scenario==5 && !strcmp(name,"CONFIG_ALIAS_END"))return -1;
    if(scenario==7 && !strcmp(name,"CONFIG_SOCKET_BEGIN"))return -1;
    return 0;
}
int sceNetInetSocket(int domain,int type,int protocol)
{ assert(socket_begin==1 && domain==AF_INET && type==SOCK_DGRAM && !protocol);if(scenario==1)return -1;opened++;return 7; }
int sceNetInetClose(int fd) { assert(fd==7);closed++;return scenario==6 ? -1 : 0; }
static int native_ioctl(int fd,unsigned command,void *data)
{
    assert(fd==7 && socket_end==1 && begin==1 && !end && command==0x8040691a);
    static const unsigned char expected[64]={
        'u','s','b',0,0,0,0,0,0,0,0,0,0,0,0,0,
        16,2,0,0,192,168,77,2,0,0,0,0,0,0,0,0,
        16,2,0,0,192,168,77,255,0,0,0,0,0,0,0,0,
        16,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0
    };
    assert(!memcmp(data,expected,sizeof(expected)));ioctls++;
    return scenario==2 ? -1 : 0;
}
vnet_ioctl_fn vnet_resolve_ioctl(void) { return scenario==3 ? 0 : native_ioctl; }
int main(int argc,char **argv)
{
    assert(argc==2);scenario=atoi(argv[1]);
    int r=vnet_configure_lab(report);assert(r==(scenario==0 ? 0 : -1));assert(opened==closed);
    if(scenario==0)assert(ioctls==1 && end==1);
    if(scenario==3 || scenario==7)assert(!opened && !ioctls);
    if(scenario==4)assert(!ioctls && closed==1);
    return 0;
}
