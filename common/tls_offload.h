/* Private USB TLS tunnel. Upstream verification is mandatory, never negotiated. */
#ifndef PSP_TLS_OFFLOAD_H
#define PSP_TLS_OFFLOAD_H
#include <stdint.h>
#include <stddef.h>
#define TLS_OFFLOAD_PORT 27314
#define TLS_OFFLOAD_HOST_MAX 253
#define TLS_OFFLOAD_SLOTS 2
#define TLS_OFFLOAD_HEADER 8
#define TLS_OFFLOAD_REPLY 16
static inline int tls_offload_host(const char *host,unsigned length)
{
    if(!length || length>TLS_OFFLOAD_HOST_MAX)return 0;
    if(host[0]=='.' || host[length-1]=='.' || host[0]=='-' || host[length-1]=='-')return 0;
    unsigned label=0;int letter=0;
    for(unsigned i=0;i<length;++i) {
        unsigned c=(unsigned char)host[i];
        if(c=='.'){if(!label || host[i-1]=='-')return 0;label=0;continue;}
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-'))return 0;
        if((!label && c=='-') || ++label>63)return 0;
        if((c>='a' && c<='z') || (c>='A' && c<='Z'))letter=1;
    }
    return label && letter; /* DNS identities only in v1, no IP literal guessing. */
}
static inline void tls_offload_request(uint8_t header[8],unsigned length,unsigned port)
{
    header[0]='T';header[1]='L';header[2]='S';header[3]='1';
    header[4]=length;header[5]=length>>8;header[6]=port;header[7]=port>>8;
}
static inline int tls_offload_request_valid(const uint8_t h[8])
{return h[0]=='T' && h[1]=='L' && h[2]=='S' && h[3]=='1' && (h[4]|(unsigned)h[5]<<8)>0 && (h[4]|(unsigned)h[5]<<8)<=TLS_OFFLOAD_HOST_MAX && h[6]==0xbb && h[7]==1;}
static inline void tls_offload_reply(uint8_t h[8],int32_t status)
{h[0]='T';h[1]='L';h[2]='R';h[3]='1';for(unsigned i=0;i<4;++i)h[4+i]=(uint32_t)status>>(8*i);}
static inline void tls_offload_reply_details(uint8_t h[16],int32_t status,int32_t detail,uint32_t flags)
{tls_offload_reply(h,status);for(unsigned i=0;i<4;++i){h[8+i]=(uint32_t)detail>>(8*i);h[12+i]=flags>>(8*i);}}
static inline uint32_t tls_offload_word(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline int tls_offload_reply_status(const uint8_t h[8],int32_t *status)
{
    if(h[0]!='T' || h[1]!='L' || h[2]!='R' || h[3]!='1')return 0;
    *status=(int32_t)((uint32_t)h[4]|(uint32_t)h[5]<<8|(uint32_t)h[6]<<16|(uint32_t)h[7]<<24);return 1;
}
#endif
