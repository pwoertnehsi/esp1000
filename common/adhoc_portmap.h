#ifndef ESP_ADHOC_PORTMAP_H
#define ESP_ADHOC_PORTMAP_H
#include <stdint.h>
#include <string.h>
#define ADHOC_PORTMAP_PORT 27313
#define ADHOC_PORTMAP_BYTES 16
#define ADHOC_PORTMAP_ADD 1
#define ADHOC_PORTMAP_REMOVE 2
#define ADHOC_PORTMAP_CLEAR 3
static inline uint32_t adhoc_map_u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void adhoc_map_put(uint8_t *p,uint32_t n)
{for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(i*8));}
static inline void adhoc_map_request(uint8_t p[16],unsigned op,unsigned proto,unsigned port,uint32_t nonce)
{memset(p,0,16);memcpy(p,"APM1",4);p[4]=op;p[5]=proto;p[6]=port;p[7]=port>>8;adhoc_map_put(p+8,nonce);}
static inline int adhoc_map_valid(const uint8_t *p,unsigned n)
{
    if(n!=16 || memcmp(p,"APM1",4) || !adhoc_map_u32(p+8) || adhoc_map_u32(p+12))return 0;
    if(p[4]==ADHOC_PORTMAP_CLEAR)return !p[5] && !p[6] && !p[7];
    return (p[4]==ADHOC_PORTMAP_ADD || p[4]==ADHOC_PORTMAP_REMOVE) &&
        (p[5]==6 || p[5]==17) && (p[6] || p[7]);
}
static inline int adhoc_map_reply(const uint8_t request[16],const uint8_t *reply,unsigned n,int32_t *status)
{
    if(n!=16 || memcmp(request,reply,4) || reply[4]!=(request[4]|0x80) ||
       memcmp(request+5,reply+5,7))return 0;
    *status=(int32_t)adhoc_map_u32(reply+12);return 1;
}
#endif
