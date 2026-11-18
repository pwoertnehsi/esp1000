/* Owned HTTP import callbacks. Native SSL objects/BIOs retain their lifecycle;
 * modern TLS and peer verification run on the ESP, never as a success stub. */
#include <pspkernel.h>
#include <pspnet_inet.h>
#include <psploadcore.h>
#include <kubridge.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include "../tls_control.h"
#include "../../common/tls_offload.h"
static wifi_tls_control_t tls={.magic=WIFI_TLS_MAGIC,.abi=WIFI_TLS_ABI,.owner=-1};
static struct {int fd,verified,error,connecting,closing;unsigned generation;void *bio,*ssl;} slots[TLS_OFFLOAD_SLOTS];
static unsigned generation;
static SceUID mutex=-1;
extern int wifiTlsRegister(unsigned,unsigned);
extern void wifiTlsConnectThunk(void);
static void enter(void){sceKernelWaitSema(mutex,1,NULL);}
static void leave(void){sceKernelSignalSema(mutex,1);}
static int find_fd(int fd){for(unsigned i=0;i<TLS_OFFLOAD_SLOTS;++i)if(slots[i].fd==fd)return i;return -1;}
static int find_ssl(void *ssl){if(ssl)for(unsigned i=0;i<TLS_OFFLOAD_SLOTS;++i)if(slots[i].ssl==ssl)return i;return -1;}
static void clear(unsigned i){memset(&slots[i],0,sizeof(slots[i]));slots[i].fd=-1;if(tls.active)--tls.active;}
static void log_result(const char *stage,int result)
{
    char line[112];unsigned n=0;
    const char *label="TLS_OFFLOAD ";while(*label)line[n++]=*label++;
    while(*stage && n<32)line[n++]=*stage++;
    const char *names[]={" result="," hooks="," active="};
    unsigned values[]={(unsigned)result,(unsigned)tls.status,tls.active};
    const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<3;++i){label=names[i];while(*label)line[n++]=*label++;for(unsigned j=0;j<8;++j)line[n++]=hex[(values[i]>>(28-4*j))&15];}
    line[n++]='\n';
    int fd=sceIoOpen("ms0:/ssl-offload.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd>=0){sceIoWrite(fd,line,n);sceIoClose(fd);}
}
static int would_block(void){int error=sceNetInetGetErrno();return error==11 || error==35 || error==119;}
static int transfer(int fd,void *buffer,unsigned size,int sending,unsigned slot,unsigned epoch)
{
    unsigned began=sceKernelGetSystemTimeLow(),done=0;
    while(done<size) {
        enter();
        if(slots[slot].generation!=epoch || slots[slot].closing || slots[slot].fd!=fd){leave();return -1;}
        int n=sending?sceNetInetSend(fd,(char *)buffer+done,size-done,MSG_DONTWAIT):sceNetInetRecv(fd,(char *)buffer+done,size-done,MSG_DONTWAIT);
        int retry=n<0 && would_block();leave();
        if(n>0){done+=n;continue;}
        if(!n || !retry || sceKernelGetSystemTimeLow()-began>=25000000u)return -1;
        sceKernelDelayThread(1000);
    }
    return 0;
}
int wifiTlsConnect(int fd,void *address,unsigned timeout,int *error,int *syserror,void *request,void *connection)
{
    (void)request;
    int (*original)(int,void *,unsigned,int *,int *)=(void *)tls.originals[WT_CONNECT];
    unsigned conn=(unsigned)connection;
    if(conn<0x08800000u || conn>0x0bfff000u || (conn&3))return -1;
    if(*(unsigned *)(conn+4)!=1)return original(fd,address,timeout,error,syserror);
    int slot=-1,result=-1;unsigned epoch=0;char host[TLS_OFFLOAD_HOST_MAX+1];unsigned length=0;
    /* The owned state+0x38 is the original hostname used by inet_pton/resolver.
     * Proxy configuration is deliberately refused, rather than misbinding SNI. */
    if(*(unsigned *)(conn+0xc0)==1)goto fail;
    const char *name=*(const char **)(conn+0x38);unsigned pointer=(unsigned)name;
    if(pointer<0x08800000u || pointer>0x0c000000u-sizeof(host))goto fail;
    while(length<sizeof(host) && name[length]){host[length]=name[length];++length;}
    if(!tls_offload_host(host,length))goto fail;
    host[length]=0;
    if(((struct sockaddr_in *)address)->sin_port!=__builtin_bswap16(443))goto fail;
    enter();
    for(unsigned i=0;i<TLS_OFFLOAD_SLOTS;++i)if(slots[i].fd<0 && !slots[i].ssl){slot=i;slots[i].fd=fd;slots[i].error=1;slots[i].connecting=1;epoch=slots[i].generation=++generation;++tls.active;break;}
    leave();if(slot<0)goto fail;
    struct sockaddr_in local;memset(&local,0,sizeof(local));local.sin_len=sizeof(local);local.sin_family=AF_INET;
    local.sin_port=__builtin_bswap16(TLS_OFFLOAD_PORT);unsigned char ip[4]={192,168,77,1};memcpy(&local.sin_addr,ip,4);
    result=original(fd,&local,timeout,error,syserror);if(result<0)goto fail;
    unsigned char header[TLS_OFFLOAD_REPLY];tls_offload_request(header,length,443);
    if(transfer(fd,header,8,1,slot,epoch) || transfer(fd,host,length,1,slot,epoch) || transfer(fd,header,TLS_OFFLOAD_REPLY,0,slot,epoch)){result=-2;goto fail;}
    int32_t status;
    if(!tls_offload_reply_status(header,&status)){result=-3;goto fail;}
    int32_t detail=(int32_t)tls_offload_word(header+8);unsigned cert_flags=tls_offload_word(header+12);
    if(status || detail || cert_flags){log_result("TLS_DETAIL",detail);log_result("CERT_FLAGS",cert_flags);result=status?status:-4;goto fail;}
    enter();
    if(slots[slot].closing || slots[slot].generation!=epoch){leave();result=-6;goto fail;}
    slots[slot].connecting=0;slots[slot].verified=1;slots[slot].error=0;leave();
    log_result("VERIFIED",0);return 0;
fail:
    if(slot>=0){
        enter();int deferred=slots[slot].closing;clear(slot);leave();
        if(deferred){int (*close_fn)(int)=(void *)tls.originals[WT_CLOSE];close_fn(fd);}
    }
    if(error)*error=5;
    if(syserror)*syserror=0;
    log_result("CONNECT_FAILED",result);return (int)0x80431075u;
}
static void *bio_new(int fd,int close_flag)
{
    enter();int i=find_fd(fd);unsigned epoch=i>=0?slots[i].generation:0;leave();
    void *(*original)(int,int)=(void *)tls.originals[WT_BIO];void *bio=original(fd,close_flag);
    enter();if(i>=0 && slots[i].fd==fd && slots[i].generation==epoch && !slots[i].closing)slots[i].bio=bio;leave();return bio;
}
static void set_bio(void *ssl,void *read_bio,void *write_bio)
{
    int slot=-1;unsigned epoch=0;
    enter();for(unsigned i=0;i<TLS_OFFLOAD_SLOTS;++i)if(read_bio && read_bio==write_bio && slots[i].bio==read_bio && slots[i].fd>=0 && !slots[i].closing){slot=i;epoch=slots[i].generation;break;}leave();
    void (*original)(void *,void *,void *)=(void *)tls.originals[WT_SETBIO];original(ssl,read_bio,write_bio);
    enter();if(slot>=0 && slots[slot].generation==epoch && slots[slot].fd>=0 && !slots[slot].closing)slots[slot].ssl=ssl;leave();
    if(slot>=0)log_result("SSL_BOUND",slot);
}
static int handshake(void *ssl)
{
    enter();int i=find_ssl(ssl),result=i>=0 && slots[i].verified && slots[i].fd>=0 && !slots[i].closing;leave();
    log_result("HANDSHAKE_READY",result?0:-1);
    return result?1:-1; /* No legacy TLS fallback on an unbound/reused socket. */
}
static int io(void *ssl,void *data,int length,int sending)
{
    enter();int i=find_ssl(ssl),fd=i>=0?slots[i].fd:-1;
    if(i<0 || fd<0 || slots[i].closing || length<0 || (!data && length)) {if(i>=0)slots[i].error=1;leave();return -1;}
    /* Hold only across one nonblocking NET call, so close cannot reuse its fd. */
    int n=sending?sceNetInetSend(fd,data,length,MSG_DONTWAIT):sceNetInetRecv(fd,data,length,MSG_DONTWAIT);
    int error=n>0?0:n==0 && !sending?6:would_block()?(sending?3:2):1;
    slots[i].error=error;leave();return n;
}
static int read_ssl(void *ssl,void *data,int length){return io(ssl,data,length,0);}
static int write_ssl(void *ssl,const void *data,int length){return io(ssl,(void *)data,length,1);}
static int get_error(void *ssl,int result)
{
    enter();int i=find_ssl(ssl),error=i>=0?slots[i].error:1;leave();
    return result>0 && i>=0?0:error;
}
static int ready(void *ssl)
{
    enter();int i=find_ssl(ssl),result=i>=0 && slots[i].verified && slots[i].fd>=0 && !slots[i].closing;leave();return result;
}
static int shutdown_ssl(void *ssl)
{
    enter();int i=find_ssl(ssl);if(i>=0 && slots[i].fd>=0)sceNetInetShutdown(slots[i].fd,1);leave();return 1;
}
static void free_ssl(void *ssl)
{
    enter();int i=find_ssl(ssl);if(i>=0){slots[i].ssl=NULL;slots[i].bio=NULL;if(slots[i].fd<0)clear(i);}leave();
    void (*original)(void *)=(void *)tls.originals[WT_FREE];original(ssl);
}
static int close_socket(int fd)
{
    enter();int i=find_fd(fd);
    if(i>=0 && slots[i].connecting){slots[i].closing=1;sceNetInetShutdown(fd,2);leave();return 0;}
    if(i>=0){slots[i].fd=-1;slots[i].verified=0;slots[i].error=1;slots[i].bio=NULL;if(!slots[i].ssl)clear(i);}leave();
    int (*original)(int)=(void *)tls.originals[WT_CLOSE];return original(fd);
}
int wifi_tls_init(void)
{
    mutex=sceKernelCreateSema("ESPTlsSlots",0,1,1,NULL);if(mutex<0)return -1;
    for(unsigned i=0;i<TLS_OFFLOAD_SLOTS;++i)slots[i].fd=-1;
    unsigned callbacks[]={(unsigned)wifiTlsConnectThunk,(unsigned)bio_new,(unsigned)set_bio,(unsigned)handshake,(unsigned)read_ssl,(unsigned)write_ssl,(unsigned)get_error,(unsigned)ready,(unsigned)shutdown_ssl,(unsigned)free_ssl,(unsigned)close_socket};
    memcpy(tls.callbacks,callbacks,sizeof(callbacks));int result=wifiTlsRegister((unsigned)&tls,sizeof(tls));log_result("REGISTER",result);return result;
}
int wifi_tls_can_stop(void)
{
    if(tls.active)return 0;
    if(tls.owner>=0){SceModule m;if(kuKernelFindModuleByName("SceHttp_Library",&m)>=0 && m.modid==tls.owner)return 0;}
    return 1;
}
