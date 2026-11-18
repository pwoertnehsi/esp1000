#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../virtual_net/driver/log_queue.h"
#include "../common/packet_trace.h"
#include "../virtual_net/driver/frame_queue.h"
int main(void)
{
    vnet_log_queue_t queue={0};char out[VNET_LOG_WIDTH],text[400];
    memset(text,'x',sizeof(text));text[399]=0;
    for(unsigned cycle=0;cycle<1000;++cycle) {
        for(unsigned i=0;i<VNET_LOG_ROWS;++i)assert(vnet_log_push(&queue,text));
        assert(!vnet_log_push(&queue,"overflow"));
        for(unsigned i=0;i<VNET_LOG_ROWS;++i){assert(vnet_log_pop(&queue,out));assert(strlen(out)==223);}
        assert(!vnet_log_pop(&queue,out));
    }
    queue.read=queue.write=UINT_MAX-15;
    for(unsigned i=0;i<32;++i)assert(vnet_log_push(&queue,"wrap"));
    for(unsigned i=0;i<32;++i)assert(vnet_log_pop(&queue,out) && !strcmp(out,"wrap"));
    assert(queue.dropped==1000);
    unsigned char frame[54]={0};frame[12]=8;frame[14]=0x45;frame[17]=40;frame[23]=6;
    frame[26]=192;frame[27]=168;frame[28]=77;frame[29]=2;frame[30]=192;frame[31]=168;frame[32]=77;frame[33]=1;
    frame[34]=0x12;frame[35]=0x34;frame[37]=80;frame[38]=1;frame[39]=2;frame[40]=3;frame[41]=4;
    frame[46]=0x50;frame[47]=2;
    unsigned checksum=65535-trace_sum(frame+14,20,0);frame[24]=checksum>>8;frame[25]=checksum;
    checksum=65535-trace_sum(frame+34,20,trace_sum(frame+26,8,26));frame[50]=checksum>>8;frame[51]=checksum;
    packet_trace_t p;assert(packet_trace_parse(&p,frame,54) && p.sport==0x1234 && p.dport==80 && p.seq==0x01020304 && p.flags==2 && p.ip_ok && p.tcp_ok && !p.payload);
    for(unsigned n=0;n<54;++n)assert(!packet_trace_parse(&p,frame,n));
    frame[51]^=1;assert(packet_trace_parse(&p,frame,54) && !p.tcp_ok);
    frame[46]=0xf0;assert(!packet_trace_parse(&p,frame,54));
    frame[14]=0x4f;assert(!packet_trace_parse(&p,frame,54));
    vnet_frame_queue_t frames={0};unsigned char input[VNET_FRAME_CAPACITY],output[VNET_FRAME_CAPACITY];
    for(unsigned n=14;n<=VNET_FRAME_CAPACITY;++n) {
        memset(input,n,sizeof(input));
        for(unsigned i=0;i<8;++i)assert(vnet_frame_push(&frames,input,n));
        assert(!vnet_frame_push(&frames,input,n));
        /* A blocked native consumer leaves at most eight byte-owned frames.
         * Full ingress must refuse immediately, preserving USB service. */
        for(unsigned i=0;i<8;++i)assert(vnet_frame_pop(&frames,output)==n && !memcmp(input,output,n));
        assert(!vnet_frame_pop(&frames,output));
    }
    assert(!vnet_frame_push(&frames,input,13) && !vnet_frame_push(&frames,input,1515));
    frames.read=frames.write=UINT_MAX-3;
    for(unsigned i=0;i<8;++i){input[0]=i;assert(vnet_frame_push(&frames,input,64));}
    for(unsigned i=0;i<8;++i)assert(vnet_frame_pop(&frames,output)==64 && output[0]==i);
    puts("PASS: bounded log/frame queues, all frame sizes, overflow/wrap, TCP metadata/checksums and all frame truncations");
}
