#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include "tls_client_shim.h"
#include "../wifi_plugin/menu_bridge/tls_offload.c"
static unsigned ticks,lock_depth,original_calls,closes,frees;
static int response_status,io_errno=11,blocked,app_mode,cancel_during_wait,recycle_bio;
static unsigned response_flags;
static unsigned response_at,sent_bytes;
static unsigned char sent[512];
int sceKernelWaitSema(int a,int b,void *c){(void)a;(void)b;(void)c;assert(!lock_depth);lock_depth=1;return 0;}
int sceKernelSignalSema(int a,int b){(void)a;(void)b;assert(lock_depth);lock_depth=0;return 0;}
int sceKernelCreateSema(const char *n,int a,int b,int c,void *d){(void)n;(void)a;(void)b;(void)c;(void)d;return 1;}
unsigned sceKernelGetSystemTimeLow(void){return ticks;}
int sceKernelDelayThread(unsigned n){ticks+=n;if(cancel_during_wait){cancel_during_wait=0;assert(!close_socket(5));}return 0;}
int sceIoOpen(const char *n,int a,int b){(void)n;(void)a;(void)b;assert(!lock_depth);return -1;}
int sceIoWrite(int a,const void *b,unsigned c){(void)a;(void)b;return c;}int sceIoClose(int a){(void)a;return 0;}
int sceNetInetGetErrno(void){return io_errno;}
int sceNetInetSend(int fd,const void *p,unsigned n,int flags)
{(void)fd;assert(lock_depth==1 && flags==MSG_DONTWAIT);if(app_mode==2)return n>3?3:(int)n;if(app_mode)return -1;if(n>2)n=2;assert(sent_bytes+n<=sizeof(sent));memcpy(sent+sent_bytes,p,n);sent_bytes+=n;return n;}
int sceNetInetRecv(int fd,void *p,unsigned n,int flags)
{(void)fd;assert(lock_depth==1 && flags==MSG_DONTWAIT);if(app_mode==2){if(n>3)n=3;memcpy(p,"abc",n);return n;}if(blocked || app_mode)return -1;uint8_t reply[TLS_OFFLOAD_REPLY];tls_offload_reply_details(reply,response_status,0,response_flags);if(n>2)n=2;assert(response_at+n<=TLS_OFFLOAD_REPLY);memcpy(p,reply+response_at,n);response_at+=n;return n;}
int sceNetInetShutdown(int fd,int how){assert(lock_depth && fd>=0 && (how==1 || how==2));return 0;}
int kuKernelFindModuleByName(char *n,SceModule *m){(void)n;(void)m;return -1;}
int wifiTlsRegister(unsigned address,unsigned size){assert(address==(unsigned)(uintptr_t)&tls && size==sizeof(tls));return 0;}
void wifiTlsConnectThunk(void){}
static int original_connect(int fd,void *a,unsigned timeout,int *error,int *syserror)
{(void)fd;(void)timeout;(void)error;(void)syserror;assert(!lock_depth);struct sockaddr_in *s=a;assert(s->sin_port==__builtin_bswap16(TLS_OFFLOAD_PORT));++original_calls;return 0;}
static void *original_bio(int fd,int flag){(void)flag;if(recycle_bio){enter();int i=find_fd(fd);assert(i>=0);++slots[i].generation;leave();}return (void *)(uintptr_t)(0x10000+fd);}
static void original_set(void *ssl,void *r,void *w){assert(ssl && r==w);}
static void original_free(void *ssl){assert(ssl);++frees;}
static int original_close(int fd){assert(fd>=0);++closes;return 0;}
int main(void)
{
    uint8_t *conn=mmap((void *)0x09000000,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);assert(conn!=(void *)-1);
    char *name=(void *)(conn+4096);strcpy(name,"en.touhouwiki.net");*(char **)(conn+0x38)=name;*(unsigned *)(conn+4)=1;
    struct sockaddr_in destination={.sin_port=__builtin_bswap16(443)};int error=0,syserror=0;
    assert(!wifi_tls_init());tls.status=1;
    tls.originals[WT_CONNECT]=(unsigned)(uintptr_t)original_connect;tls.originals[WT_BIO]=(unsigned)(uintptr_t)original_bio;
    tls.originals[WT_SETBIO]=(unsigned)(uintptr_t)original_set;tls.originals[WT_FREE]=(unsigned)(uintptr_t)original_free;tls.originals[WT_CLOSE]=(unsigned)(uintptr_t)original_close;
    response_status=-5;
    assert(wifiTlsConnect(4,&destination,0,&error,&syserror,NULL,conn)<0 && !tls.active);
    assert(original_calls==1 && error==5 && response_at==TLS_OFFLOAD_REPLY); /* never SSL success on certificate failure */
    response_at=sent_bytes=0;response_status=0;response_flags=1;
    assert(wifiTlsConnect(4,&destination,0,&error,&syserror,NULL,conn)<0 && !tls.active);
    response_at=sent_bytes=0;response_flags=0;
    assert(!wifiTlsConnect(4,&destination,0,&error,&syserror,NULL,conn) && tls.active==1);
    assert(tls_offload_request_valid(sent) && !memcmp(sent+8,name,strlen(name)));
    void *bio=bio_new(4,0),*ssl=(void *)0x20000;set_bio(ssl,bio,bio);assert(handshake(ssl)==1 && ready(ssl)==1);
    app_mode=1;char data[8];assert(read_ssl(ssl,data,8)<0 && get_error(ssl,-1)==2);
    assert(write_ssl(ssl,data,8)<0 && get_error(ssl,-1)==3);
    io_errno=54;assert(read_ssl(ssl,data,8)<0 && get_error(ssl,-1)==1);
    app_mode=2;
    assert(read_ssl(ssl,data,8)==3 && !memcmp(data,"abc",3) && get_error(ssl,3)==0);
    assert(write_ssl(ssl,data,8)==3 && get_error(ssl,3)==0);
    assert(!close_socket(4) && ready(ssl)==0 && handshake(ssl)<0 && tls.active==1);
    free_ssl(ssl);assert(!tls.active && closes==1 && frees==1);
    app_mode=0;io_errno=11;response_at=sent_bytes=0;
    assert(!wifiTlsConnect(4,&destination,0,&error,&syserror,NULL,conn));bio=bio_new(4,0);set_bio(ssl,bio,bio);
    free_ssl(ssl);assert(tls.active==1);assert(!close_socket(4) && !tls.active); /* reverse close/free order and fd reuse */
    for(int fd=5;fd<=6;++fd){response_at=sent_bytes=0;assert(!wifiTlsConnect(fd,&destination,0,&error,&syserror,NULL,conn));}
    unsigned full_calls=original_calls;assert(tls.active==2 && wifiTlsConnect(7,&destination,0,&error,&syserror,NULL,conn)<0 && original_calls==full_calls);
    close_socket(5);close_socket(6);assert(!tls.active);
    response_at=sent_bytes=0;assert(!wifiTlsConnect(4,&destination,0,&error,&syserror,NULL,conn));
    recycle_bio=1;bio=bio_new(4,0);recycle_bio=0;set_bio(ssl,bio,bio);assert(handshake(ssl)<0);close_socket(4);assert(!tls.active);
    unsigned calls=original_calls;strcpy(name,"bad/host");assert(wifiTlsConnect(5,&destination,0,&error,&syserror,NULL,conn)<0 && original_calls==calls);
    strcpy(name,"en.touhouwiki.net");*(unsigned *)(conn+0xc0)=1;assert(wifiTlsConnect(5,&destination,0,&error,&syserror,NULL,conn)<0 && original_calls==calls);*(unsigned *)(conn+0xc0)=0;
    blocked=1;response_at=sent_bytes=0;unsigned began=ticks;
    assert(wifiTlsConnect(5,&destination,0,&error,&syserror,NULL,conn)<0 && !tls.active && ticks-began>=25000000u);
    response_at=sent_bytes=0;cancel_during_wait=1;unsigned closes_before=closes;
    assert(wifiTlsConnect(5,&destination,0,&error,&syserror,NULL,conn)<0 && !tls.active && closes==closes_before+1);
    assert(wifi_tls_can_stop());
    assert(tls_offload_host("a-b.example",11));assert(!tls_offload_host("1.2.3.4",7));assert(!tls_offload_host("a..b",4));assert(!tls_offload_host("a-.b",4));
    munmap(conn,8192);puts("PASS: actual TLS client verified ACK, hostname binding, partial I/O, SSL errors, cleanup, timeout and descriptor reuse");
}
