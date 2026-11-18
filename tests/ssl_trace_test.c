#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ssl_trace.h"
#include "packet_trace.h"
static ssl_trace_t state;
static ssl_trace_event_t events[512];static unsigned used;
static uint32_t sequence[2];
static unsigned local_port=12345;
static void collect(void *ctx,const ssl_trace_event_t *e){(void)ctx;assert(used<512);events[used++]=*e;}
static void put16(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
static void put32(uint8_t *p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static void packet(int dir,uint32_t seq,unsigned flags,const uint8_t *data,unsigned length)
{
    uint8_t frame[1600]={0};assert(length<1500);uint8_t *ip=frame+14,*tcp=ip+20;
    frame[12]=8;ip[0]=0x45;put16(ip+2,40+length);ip[8]=64;ip[9]=6;
    put32(ip+12,dir?0x01020304:0xc0a84d02);put32(ip+16,dir?0xc0a84d02:0x01020304);
    put16(tcp,dir?443:local_port);put16(tcp+2,dir?local_port:443);put32(tcp+4,seq);tcp[12]=0x50;tcp[13]=flags;
    if(length)memcpy(tcp+20,data,length);
    put16(ip+10,(uint16_t)~trace_sum(ip,20,0));
    put16(tcp+16,(uint16_t)~trace_sum(tcp,20+length,trace_sum(ip+12,8,6+20+length)));
    ssl_trace_feed(&state,!dir,frame,54+length,collect,NULL);
}
static void send_data(int dir,const uint8_t *p,unsigned n)
{packet(dir,sequence[dir],0x18,p,n);sequence[dir]+=n;}
static void begin(void)
{
    memset(&state,0,sizeof(state));memset(events,0,sizeof(events));used=0;
    packet(0,100,2,NULL,0);packet(1,500,0x12,NULL,0);sequence[0]=101;sequence[1]=501;
}
static unsigned count(unsigned kind)
{unsigned n=0;for(unsigned i=0;i<used;++i)if(events[i].event==kind)++n;return n;}
static ssl_trace_event_t last(unsigned kind)
{for(unsigned i=used;i;--i)if(events[i-1].event==kind)return events[i-1];assert(0);return events[0];}
static unsigned hello(uint8_t *record,int client)
{
    uint8_t *body=record+9;memset(record,0,256);record[0]=22;record[1]=3;record[2]=1;record[5]=client?1:2;
    body[0]=3;body[1]=1;memset(body+2,0xab,32);body[34]=0;
    unsigned n=35;
    if(client){put16(body+n,4);n+=2;put16(body+n,0x2f);put16(body+n+2,0x35);n+=4;body[n++]=1;body[n++]=0;}
    else {put16(body+n,0x2f);n+=2;body[n++]=0;}
    record[8]=n;put16(record+3,n+4);return n+9;
}
int main(void)
{
    uint8_t buffer[256];unsigned n=hello(buffer,1);
    begin();for(unsigned i=0;i<n;++i)send_data(0,buffer+i,1);
    assert(count(SSL_T_CLIENT)==1 && last(SSL_T_CLIENT).a==0x301 && last(SSL_T_CLIENT).b==2);
    assert(count(SSL_T_CIPHER)==2 && last(SSL_T_CIPHER).a==0x35);
    packet(0,101,0x18,buffer,n);assert(count(SSL_T_CLIENT)==1); /* full retransmission */
    n=hello(buffer,0);send_data(1,buffer,n);assert(last(SSL_T_SERVER).b==0x2f);
    const uint8_t alert[]={21,3,1,0,2,2,70};send_data(1,alert,sizeof(alert));
    assert(last(SSL_T_ALERT).a==2 && last(SSL_T_ALERT).b==70);
    packet(1,sequence[1],4,NULL,0);assert(count(SSL_T_RESET)==1 && count(SSL_T_END)==1);
    assert(last(SSL_T_END).a>0 && last(SSL_T_END).b>0);
    /* Handshake header/body split across records, with overlapping TCP data. */
    n=hello(buffer,1);begin();uint8_t first[11];memcpy(first,buffer,11);put16(first+3,6);send_data(0,first,11);
    uint8_t second[256]={22,3,1};put16(second+3,n-11);memcpy(second+5,buffer+11,n-11);
    send_data(0,second,6);packet(0,sequence[0]-3,0x18,second+3,n-9);sequence[0]+=n-12;
    assert(count(SSL_T_CLIENT)==1 && !count(SSL_T_GAP));
    /* Never decode encrypted alerts or application payloads. */
    begin();const uint8_t ccs[]={20,3,1,0,1,1};send_data(1,ccs,sizeof(ccs));send_data(1,alert,sizeof(alert));
    assert(count(SSL_T_CCS)==1 && count(SSL_T_ENCRYPTED)==1 && !count(SSL_T_ALERT));
    /* SSLv2-compatible ClientHello: its offered version is not the record format. */
    begin();uint8_t ssl2[]={0x80,12,1,3,0,0,3,0,0,0,0,0,0,0x2f};
    send_data(0,ssl2,sizeof(ssl2));assert(count(SSL_T_SSL2)==1 && last(SSL_T_SSL2).a==0x300 && last(SSL_T_CIPHER).a==0x2f);
    /* A forward gap is labelled; later ciphertext cannot become a fake alert. */
    begin();packet(1,sequence[1]+10,0x18,alert,sizeof(alert));assert(count(SSL_T_GAP)==1 && !count(SSL_T_ALERT));
    /* Reject malformed lengths and stop decoding instead of reading past bounds. */
    begin();uint8_t bad[]={22,3,1,0xff,0xff};send_data(0,bad,sizeof(bad));assert(count(SSL_T_BAD)==1);
    begin();n=hello(buffer,1);buffer[9+34]=255;send_data(0,buffer,n);assert(count(SSL_T_BAD)==1 && !count(SSL_T_CLIENT));
    /* Handshake headers and bodies can span multiple TLS records. */
    begin();n=hello(buffer,1);uint8_t split[280];
    memcpy(split,buffer,5);put16(split+3,2);memcpy(split+5,buffer+5,2);
    memcpy(split+7,buffer,5);put16(split+10,n-7);memcpy(split+12,buffer+7,n-7);
    send_data(0,split,n+5);assert(count(SSL_T_CLIENT)==1);
    /* SNI is recorded as presence only; supported versions are numeric. */
    begin();n=hello(buffer,1);unsigned at=n;
    put16(buffer+at,29);at+=2;put16(buffer+at,0);put16(buffer+at+2,16);at+=4;
    put16(buffer+at,14);buffer[at+2]=0;put16(buffer+at+3,11);memcpy(buffer+at+5,"secret.test",11);at+=16;
    put16(buffer+at,43);put16(buffer+at+2,5);buffer[at+4]=4;put16(buffer+at+5,0x301);put16(buffer+at+7,0x304);at+=9;
    put16(buffer+3,at-5);buffer[8]=at-9;send_data(0,buffer,at);
    assert(last(SSL_T_CLIENT).c==3 && last(SSL_T_VERSIONS).a==0x304);
    for(unsigned i=0;i<SSL_TRACE_PREFIX;++i)assert(!state.flow[0].stream[0].prefix[i]);
    /* SSLv2-compatible ClientHello framing does not imply SSLv2 negotiation. */
    begin();uint8_t old[30]={0x80,28,1,3,1,0,3,0,0,0,16,0,0,0x2f};
    send_data(0,old,sizeof(old));assert(last(SSL_T_SSL2).a==0x301 && last(SSL_T_CIPHER).a==0x2f);
    begin();packet(0,108,0x18,alert,sizeof(alert));assert(count(SSL_T_GAP)==1 && !count(SSL_T_ALERT));
    packet(1,sequence[1],4,NULL,0);assert(count(SSL_T_RESET)==1 && count(SSL_T_END)==1);
    /* Flood bounded events while still preserving terminal metadata. */
    begin();const uint8_t empty[]={23,3,3,0,0};
    for(unsigned i=0;i<100;++i)send_data(0,empty,sizeof(empty));
    assert(count(SSL_T_LIMIT)==1 && used==SSL_TRACE_EVENTS);
    ssl_trace_finish(&state,collect,NULL);assert(count(SSL_T_END)==1);
    begin();
    for(unsigned i=1;i<8;++i){local_port=12345+i;packet(0,100+i,2,NULL,0);}
    assert(state.serial==8 && state.evictions==4);
    ssl_trace_finish(&state,collect,NULL);assert(count(SSL_T_END)==8);local_port=12345;
    /* Valid TCP checksums around arbitrary payloads exercise record boundaries. */
    uint32_t random=1;
    for(unsigned i=0;i<10000;++i) {
        begin();unsigned size=i%sizeof(buffer);
        for(unsigned j=0;j<size;++j){random=random*1664525+1013904223;buffer[j]=random>>24;}
        send_data(0,buffer,size);ssl_trace_finish(&state,collect,NULL);
    }
    puts("PASS: actual PSP TLS capture segmentation, overlap, SSL2 hello, alerts, encryption, gaps, limits and malformed records");
}
