#ifndef PSP_PACKET_TRACE_H
#define PSP_PACKET_TRACE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {uint8_t source[4],destination[4];unsigned protocol,sport,dport,flags,payload,ip_ok,tcp_ok;uint32_t seq,ack;} packet_trace_t;
static inline unsigned trace_be16(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static inline uint32_t trace_be32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static inline unsigned trace_sum(const uint8_t *p,unsigned n,unsigned sum)
{while(n>1){sum+=trace_be16(p);p+=2;n-=2;}if(n)sum+=(unsigned)*p<<8;while(sum>>16)sum=(sum&65535)+(sum>>16);return sum;}
static inline int packet_trace_parse(packet_trace_t *out,const uint8_t *frame,size_t length)
{
    memset(out,0,sizeof(*out));
    if(length<34 || frame[12]!=8 || frame[13] || frame[14]>>4!=4)return 0;
    const uint8_t *ip=frame+14;unsigned ihl=(ip[0]&15)*4,total=trace_be16(ip+2);
    if(ihl<20 || total<ihl || total>length-14)return 0;
    memcpy(out->source,ip+12,4);memcpy(out->destination,ip+16,4);out->protocol=ip[9];
    out->ip_ok=trace_sum(ip,ihl,0)==65535;out->payload=total-ihl;out->tcp_ok=2;
    if(out->protocol==17 && !(trace_be16(ip+6)&0x3fff) && total-ihl>=8) {
        const uint8_t *udp=ip+ihl;unsigned size=trace_be16(udp+4);
        if(size<8 || size>total-ihl)return 0;
        out->sport=trace_be16(udp);out->dport=trace_be16(udp+2);out->payload=size-8;
    }
    if(out->protocol!=6 || (trace_be16(ip+6)&0x3fff) || total-ihl<20)return 1;
    const uint8_t *tcp=ip+ihl;unsigned thl=(tcp[12]>>4)*4;
    if(thl<20 || thl>total-ihl)return 0;
    out->sport=trace_be16(tcp);out->dport=trace_be16(tcp+2);out->seq=trace_be32(tcp+4);
    out->ack=trace_be32(tcp+8);out->flags=tcp[13];out->payload=total-ihl-thl;
    unsigned sum=trace_sum(ip+12,8,6+total-ihl);
    out->tcp_ok=trace_sum(tcp,total-ihl,sum)==65535;return 1;
}
#endif
