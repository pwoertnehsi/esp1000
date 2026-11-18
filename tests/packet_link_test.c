#include "packet_link.h"
#include "lab_gateway.h"
#include <assert.h>
#include <stdio.h>
static uint32_t random_state=1;
static unsigned random_byte(void) { random_state=random_state*1664525+1013904223; return random_state>>24; }
static int transfer(packet_link_t *sender,packet_link_t *receiver,const unsigned char *data,unsigned n)
{
    assert(packet_link_begin(sender,data,n,123));
    assert(!packet_link_begin(sender,data,n,124)); /* bounded backpressure */
    unsigned frames=0; int result=0;
    link_frame_t f;
    while(packet_link_next(sender,&f,frames+1)) {
        ++frames;
        assert(link_valid(&f,sizeof(f)));
        result=packet_link_receive(receiver,&f);
        assert(result>=0);
        if(sender->total) assert(!result);
    }
    assert(frames==(n+PACKET_CHUNK-1)/PACKET_CHUNK);
    assert(result==(int)n && !memcmp(receiver->data,data,n));
    return result;
}
int main(void)
{
    packet_link_t a={0},b={0}; unsigned char data[PACKET_MAX];
    for(unsigned n=14;n<=PACKET_MAX;++n) {
        for(unsigned j=0;j<n;++j) data[j]=random_byte();
        transfer(&a,&b,data,n);
    }
    link_frame_t first,second;
    assert(!packet_link_begin(&a,data,13,1));
    assert(!packet_link_begin(&a,data,PACKET_MAX+1,1));
    assert(!packet_link_begin(&a,data,64,0));
    assert(packet_link_begin(&a,data,100,1));
    assert(packet_link_next(&a,&first,1)); assert(packet_link_next(&a,&second,2));
    assert(packet_link_receive(&b,&second)==-1); /* missing first */
    assert(!packet_link_receive(&b,&first));
    assert(!packet_link_receive(&b,&first)); /* offset-zero restarts */
    assert(!packet_link_receive(&b,&second));
    assert(packet_link_receive(&b,&second)==-1); /* duplicate noninitial */
    a.total=0;
    assert(packet_link_begin(&a,data,64,2));
    packet_link_next(&a,&first,1); packet_link_next(&a,&second,2);
    first.payload[12]^=1;
    assert(!packet_link_receive(&b,&first)); assert(packet_link_receive(&b,&second)==-1); /* CRC */
    for(unsigned i=0;i<100000;++i) {
        link_frame_t f;
        for(unsigned j=0;j<sizeof(f);++j) ((unsigned char *)&f)[j]=random_byte();
        if(i&1) { memcpy(f.magic,"PSPW",4); f.version=1; f.type=LINK_PACKET; f.length=random_byte()%49; }
        assert(packet_link_receive(&b,&f)<=0);
    }
    /* PSP Ethernet ARP -> framed USB -> ESP fixture -> framed USB -> PSP. */
    unsigned char arp[42]={0};
    memset(arp,255,6); memcpy(arp+6,vnet_lab_psp_mac,6);
    arp[12]=8; arp[13]=6; arp[15]=1; arp[16]=8; arp[18]=6; arp[19]=4; arp[21]=1;
    memcpy(arp+22,vnet_lab_psp_mac,6); memcpy(arp+28,vnet_lab_psp_ip,4);
    memcpy(arp+38,vnet_lab_gateway_ip,4);
    a.total=b.total=0;
    transfer(&a,&b,arp,sizeof(arp));
    unsigned char reply[PACKET_MAX];
    unsigned n=vnet_lab_reply(b.data,sizeof(arp),reply,sizeof(reply));
    assert(n==42); transfer(&a,&b,reply,n);
    assert(!memcmp(b.data,vnet_lab_psp_mac,6) && b.data[21]==2);
    puts("PASS: 1501 packet sizes, fragmentation/CRC/backpressure/reset, 100k malformed records, framed ESP ARP roundtrip");
}
