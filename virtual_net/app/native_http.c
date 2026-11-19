#include <pspkernel.h>
#include <pspnet_inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include "native_config.h"
#include "native_http.h"
#include "native_route.h"
#include "http_response.h"
#include "lab_gateway.h"
#define PAGE_PATH "ms0:/native-http.html"
#include "../../common/page_file.h"

static int wait_socket(int socket,int events,unsigned deadline,
                       int (*report)(const char *,int),const char *phase)
{
    unsigned last=0;
    while((int)(deadline-sceKernelGetSystemTimeLow())>0) {
        unsigned now=sceKernelGetSystemTimeLow();
        if(!last || now-last>=1000000u) {
            if(report(phase,0)<0)return -1;
            last=now;
        }
        struct SceNetInetPollfd p={socket,events,0};
        int r=sceNetInetPoll(&p,1,200);
        if(r<0)return -1;
        if(r==1) {
            if(p.revents&(SCE_NET_INET_POLLERR|SCE_NET_INET_POLLNVAL))return -1;
            if(p.revents&events)return 0;
            if((events&SCE_NET_INET_POLLIN) && (p.revents&SCE_NET_INET_POLLHUP))return 0;
            if(p.revents&SCE_NET_INET_POLLHUP)return -1;
        }
    }
    report("HTTP_TIMEOUT",-1);return -1;
}
int vnet_install_default_route(int (*report)(const char *,int))
{
    unsigned char message[VNET_ROUTE_SIZE],reply[1024];
    vnet_default_route(message);
    if(report("ROUTE_SOCKET_BEGIN",0)<0)return -1;
    int socket=sceNetInetSocket(17,SOCK_RAW,0),result=-1;
    if(report("ROUTE_SOCKET_END",socket)<0 || socket<0)goto done;
    if(report("ROUTE_ADD_BEGIN",0)<0)goto done;
    int n=(int)sceNetInetSend(socket,message,sizeof(message),MSG_DONTWAIT);
    if(report("ROUTE_SEND",n)<0 || n!=sizeof(message))goto done;
    unsigned deadline=sceKernelGetSystemTimeLow()+3000000u;
    for(unsigned tries=0;tries<16;++tries) {
        if(wait_socket(socket,SCE_NET_INET_POLLIN,deadline,report,"ROUTE_WAIT")<0)goto done;
        n=(int)sceNetInetRecv(socket,reply,sizeof(reply),MSG_DONTWAIT);
        if(n<76 || reply[2]!=3 || reply[3]!=1 || ((unsigned)reply[0]|(unsigned)reply[1]<<8)>(unsigned)n)goto done;
        if(link_get_u32(reply+20)!=VNET_ROUTE_SEQ)continue;
        unsigned error=link_get_u32(reply+24);
        if(report("ROUTE_ACK_ERRNO",error)>=0 && !error)result=0;
        break;
    }
done:
    if(socket>=0) { int r=sceNetInetClose(socket);report("ROUTE_SOCKET_CLOSE",r);if(r)result=-1; }
    return result;
}
static int download(int (*report)(const char *,int),const unsigned char remote_ip[4],unsigned port,int internet,int bind_source)
{
    int socket=-1,result=-1,staging=0;
    unsigned char *buffer=NULL;
    if(!remote_ip[0] || remote_ip[0]>=224 || report("HTTP_CONFIG_BEGIN",0)<0)return -1;
    int r=vnet_configure_lab(report);
    if(report("HTTP_CONFIG_END",r)<0 || r || (internet && vnet_install_default_route(report)<0))goto done;
    socket=sceNetInetSocket(AF_INET,SOCK_STREAM,0);
    if(report("HTTP_SOCKET",socket)<0 || socket<0)goto done;
    int nonblocking=1;
    r=sceNetInetSetsockopt(socket,SOL_SOCKET,SO_NONBLOCK,&nonblocking,sizeof(nonblocking));
    if(report("HTTP_NONBLOCK",r)<0 || r)goto done;
    struct sockaddr_in local={0},remote={0};
#ifdef __PSP__
    local.sin_len=sizeof(local);remote.sin_len=sizeof(remote);
#endif
    local.sin_family=remote.sin_family=AF_INET;
    memcpy(&local.sin_addr,vnet_lab_psp_ip,4);
    memcpy(&remote.sin_addr,remote_ip,4);remote.sin_port=htons(port);
    if(bind_source) {
        r=sceNetInetBind(socket,(void *)&local,sizeof(local));
        if(report("HTTP_BIND",r)<0 || r)goto done;
    } else if(report("HTTP_AUTOMATIC_SOURCE",0)<0)goto done;
    if(report("HTTP_CONNECT_BEGIN",0)<0)goto done;
    r=sceNetInetConnect(socket,(void *)&remote,sizeof(remote));
    report("HTTP_CONNECT_RESULT",r);
    unsigned deadline=sceKernelGetSystemTimeLow()+15000000u;
    if(r<0 && wait_socket(socket,SCE_NET_INET_POLLOUT,deadline,report,"HTTP_CONNECT_WAIT")<0)goto done;
    int error=-1;socklen_t error_size=sizeof(error);
    r=sceNetInetGetsockopt(socket,SOL_SOCKET,SO_ERROR,&error,&error_size);
    if(report("HTTP_CONNECT_ERRNO",error)<0 || r || error_size!=sizeof(error) || error)goto done;
    static const char request[]="GET / HTTP/1.0\r\nHost: " LINK_TEST_HOST "\r\nConnection: close\r\nAccept-Encoding: identity\r\nUser-Agent: PSP-E1000-native/1\r\n\r\n";
    unsigned sent=0;
    while(sent<sizeof(request)-1) {
        if(wait_socket(socket,SCE_NET_INET_POLLOUT,deadline,report,"HTTP_SEND_WAIT")<0)goto done;
        r=(int)sceNetInetSend(socket,request+sent,sizeof(request)-1-sent,MSG_DONTWAIT);
        if(r<=0 || (unsigned)r>sizeof(request)-1-sent)goto done;
        sent+=r;
    }
    if(report("HTTP_REQUEST_SENT",sent)<0)goto done;
    size_t capacity=NATIVE_HTTP_HEADER_MAX+NATIVE_HTTP_BODY_MAX+1u,used=0;
#ifdef NATIVE_HTTP_LOCAL_ONLY
    static unsigned char local_buffer[2048];
    capacity=sizeof(local_buffer);buffer=local_buffer;
#else
    buffer=malloc(capacity);
#endif
    if(!buffer)goto done;
    deadline=sceKernelGetSystemTimeLow()+30000000u;
    for(;;) {
        if(used==capacity)goto done;
        if(wait_socket(socket,SCE_NET_INET_POLLIN,deadline,report,"HTTP_RECEIVE_WAIT")<0)goto done;
        size_t wanted=capacity-used;if(wanted>2048)wanted=2048;
        r=(int)sceNetInetRecv(socket,buffer+used,wanted,MSG_DONTWAIT);
        if(r<0 || (size_t)r>wanted)goto done;
        if(!r)break;
        used+=r;
        if(report("HTTP_RECEIVED_BYTES",used)<0)goto done;
    }
    size_t offset,length;
    if(!http_response_body(buffer,used,&offset,&length)) {report("HTTP_RESPONSE_REFUSED",-1);goto done;}
    if(report("HTTP_BODY_BYTES",length)<0)goto done;
    if(!internet) {
        static const char marker[]="PSP native TCP probe";
        int found=0;
        for(size_t i=0;i+sizeof(marker)-1<=length;++i)
            if(!memcmp(buffer+offset+i,marker,sizeof(marker)-1)) {found=1;break;}
        if(!found) {report("LOCAL_TCP_MARKER_REFUSED",-1);goto done;}
        result=report("LOCAL_TCP_HTTP_OK",length)<0 ? -1 : 0;
        goto done;
    }
#ifndef NATIVE_HTTP_LOCAL_ONLY
    if(page_file_ops.begin(NULL)<0)goto done;
    staging=1;
    if(page_file_ops.write(NULL,buffer+offset,length)<0 || page_file_ops.commit(NULL)<0)goto done;
    staging=0;
    if(report("HTTP_SAVED_ms0:/native-http.html",length)>=0)result=0;
#endif
done:
#ifndef NATIVE_HTTP_LOCAL_ONLY
    if(staging)page_file_ops.abort(NULL);
    free(buffer);
#else
    (void)staging;
#endif
    if(socket>=0) {int close=sceNetInetClose(socket);report("HTTP_SOCKET_CLOSE",close);if(close)result=-1;}
    report(internet?"NATIVE_HTTP_RESULT":"LOCAL_TCP_RESULT",result);
    return result;
}

int vnet_run_native_http(int (*report)(const char *,int),const unsigned char remote_ip[4])
{ return download(report,remote_ip,80,1,1); }
int vnet_run_local_http(int (*report)(const char *,int))
{ static const unsigned char ip[4]={192,168,77,1};return download(report,ip,8080,0,1); }

int vnet_run_unbound_local_http(int (*report)(const char *,int))
{ static const unsigned char ip[4]={192,168,77,1};return download(report,ip,8080,0,0); }
