#include "native_http_shim.h"
#include "native_route.h"
#include "../virtual_net/app/native_http.h"
#include "../common/page_file.h"
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
static int scenario,opened,closed,committed,aborted,writes,route_receives;
static unsigned clock_us,sent,received;
static unsigned char remote[4]={93,184,216,34};
static const char response[]="HTTP/1.0 200 OK\r\nContent-Length: 19\r\n\r\n<html>hello</html>\n";
static int report(const char *name,int value)
{(void)value;return scenario==15 && !strcmp(name,"HTTP_CONNECT_BEGIN")?-1:0;}
unsigned sceKernelGetSystemTimeLow(void) {clock_us+=50000;return clock_us;}
int vnet_configure_lab(int (*fn)(const char *,int)) {(void)fn;return scenario==1?-1:0;}
int sceNetInetSocket(int family,int type,int proto)
{
    assert(!proto);
    if(family==17){assert(type==SOCK_RAW);if(scenario==18)return -1;opened++;return 10;}
    assert(family==AF_INET && type==SOCK_STREAM);if(scenario==19)return -1;opened++;return 11;
}
int sceNetInetClose(int s) {assert(s==10 || s==11);closed++;return scenario==16 && s==11?-1:0;}
size_t sceNetInetSend(int s,const void *data,size_t n,int flags)
{
    assert(flags==MSG_DONTWAIT);
    if(s==10){unsigned char expected[VNET_ROUTE_SIZE];vnet_default_route(expected);assert(n==sizeof(expected) && !memcmp(data,expected,n));return scenario==4?n-1:n;}
    assert(s==11);if(scenario==8 && n>5)n=5;sent+=n;return n;
}
int sceNetInetPoll(struct SceNetInetPollfd *p,size_t n,int timeout)
{
    assert(n==1 && timeout==200);clock_us+=200000;
    if(scenario==6 && p->fd==11)return 0;
    p->revents=p->events;return 1;
}
size_t sceNetInetRecv(int s,void *out,size_t capacity,int flags)
{
    assert(flags==MSG_DONTWAIT);
    if(s==10){assert(capacity>=124);unsigned char *p=out;memset(p,0,124);p[0]=124;p[2]=3;p[3]=1;link_put_u32(p+20,scenario==3?0:VNET_ROUTE_SEQ);link_put_u32(p+24,scenario==2?17:0);route_receives++;return 124;}
    assert(s==11 && capacity>=17);if(scenario==17)return (size_t)-1;
    const char *data=response;size_t length=strlen(data);
    if(scenario==22)data="HTTP/1.0 200 OK\r\n\r\n<html>PSP native TCP probe</html>",length=strlen(data);
    if(scenario==10)data="HTTP/1.0 301 Redirect\r\n\r\nwrong",length=strlen(data);
    if(scenario==11)length--;
    if(received==length)return 0;
    size_t n=length-received;if(n>capacity)n=capacity;if(scenario==9 && n>17)n=17;
    memcpy(out,data+received,n);received+=n;return n;
}
int sceNetInetSetsockopt(int s,int level,int option,const void *value,socklen_t n)
{assert(s==11 && level==SOL_SOCKET && option==SO_NONBLOCK && n==sizeof(int) && *(int *)value==1);return scenario==5?-1:0;}
int sceNetInetGetsockopt(int s,int level,int option,void *value,socklen_t *n)
{assert(s==11 && level==SOL_SOCKET && option==SO_ERROR && *n==sizeof(int));*(int *)value=scenario==7?111:0;return 0;}
int sceNetInetBind(int s,const struct sockaddr *address,socklen_t n)
{const struct sockaddr_in *a=(const void *)address;const unsigned char ip[]={192,168,77,2};assert(s==11 && n==sizeof(*a) && !memcmp(&a->sin_addr,ip,4));return scenario==20?-1:0;}
int sceNetInetConnect(int s,const struct sockaddr *address,socklen_t n)
{const struct sockaddr_in *a=(const void *)address;assert(s==11 && n==sizeof(*a) && !memcmp(&a->sin_addr,scenario>=21?(const unsigned char[]){192,168,77,1}:remote,4) && a->sin_port==htons(scenario>=21?8080:80));return -1;}
static int begin(void *ctx) {(void)ctx;return scenario==12?-1:0;}
static int write_body(void *ctx,const void *p,unsigned n)
{(void)ctx;assert(n==19 && !memcmp(p,"<html>hello</html>\n",19));writes++;return scenario==13?-1:0;}
static int commit(void *ctx) {(void)ctx;if(scenario==14)return -1;committed++;return 0;}
static void abort_body(void *ctx) {(void)ctx;aborted++;}
const link_file_ops_t page_file_ops={begin,write_body,commit,abort_body};
int main(int argc,char **argv)
{
    assert(argc==2);scenario=atoi(argv[1]);int r=scenario>=21?vnet_run_local_http(report):vnet_run_native_http(report,remote);
    int success=scenario==0 || scenario==8 || scenario==9 || scenario==22;
    assert(r==(success?0:-1));assert(opened==closed);
    if(success && scenario<21)assert(committed==1 && writes==1 && !aborted && sent>50);
    if(scenario>=21)assert(!committed && !writes && !route_receives);
    if(scenario==13 || scenario==14)assert(aborted==1 && !committed);
    if(scenario==3)assert(route_receives>0 && route_receives<=16);
}
