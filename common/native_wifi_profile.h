#ifndef PSP_NATIVE_WIFI_PROFILE_H
#define PSP_NATIVE_WIFI_PROFILE_H
#include "link_protocol.h"
#define NATIVE_PROFILE_VERSION 2u
#define NATIVE_PROFILE_BEGIN 1u
#define NATIVE_PROFILE_KEY 2u
#define NATIVE_PROFILE_COMMIT 3u
#define NATIVE_PROFILE_NETWORK 4u
/* Sony personal-network security values. No enterprise credentials cross USB. */
typedef struct {
    uint8_t ssid_length,key_length,security,reserved,ssid[32],key[64];
    uint8_t ip_mode,dns_mode,reserved_network[2];
    uint8_t ip[4],mask[4],gateway[4],dns[2][4];
} native_wifi_profile_t;
typedef struct {unsigned request,keys;native_wifi_profile_t profile;} native_profile_receiver_t;
static inline void native_profile_clear(void *data,unsigned length)
{volatile uint8_t *p=data;while(length--)*p++=0;}
static inline int native_profile_hex(const uint8_t *p,unsigned length)
{for(unsigned i=0;i<length;++i)if(!((p[i]>='0' && p[i]<='9') || (p[i]>='a' && p[i]<='f') || (p[i]>='A' && p[i]<='F')))return 0;return 1;}
static inline int native_profile_ip(const uint8_t ip[4],int zero_allowed)
{
    if(!(ip[0]|ip[1]|ip[2]|ip[3]))return zero_allowed;
    return ip[0]>0 && ip[0]<224 && ip[0]!=127;
}
static inline int native_profile_network_valid(const native_wifi_profile_t *p)
{
    if(p->ip_mode>1 || p->dns_mode>1 || p->reserved_network[0] || p->reserved_network[1])return 0;
    if(p->ip_mode) {
        uint32_t mask=(uint32_t)p->mask[0]<<24|(uint32_t)p->mask[1]<<16|(uint32_t)p->mask[2]<<8|p->mask[3];
        uint32_t host=~mask;
        if(!mask || (host&(host+1)) || !native_profile_ip(p->ip,0) || !native_profile_ip(p->gateway,1))return 0;
    } else {for(unsigned i=0;i<4;++i)if(p->ip[i] || p->mask[i] || p->gateway[i])return 0;}
    if(p->dns_mode) {
        if(!native_profile_ip(p->dns[0],0) || !native_profile_ip(p->dns[1],1))return 0;
    } else {for(unsigned i=0;i<4;++i)if(p->dns[0][i] || p->dns[1][i])return 0;}
    return 1;
}
static inline int native_profile_valid(const native_wifi_profile_t *p)
{
    if(!p || !native_profile_network_valid(p) || !p->ssid_length || p->ssid_length>32 || p->key_length>64 || p->reserved)return 0;
    for(unsigned i=0;i<p->ssid_length;++i)if(!p->ssid[i])return 0;
    for(unsigned i=p->ssid_length;i<32;++i)if(p->ssid[i])return 0;
    for(unsigned i=p->key_length;i<64;++i)if(p->key[i])return 0;
    if(p->security==0)return !p->key_length;
    if(p->security==1)return p->key_length==5 || (p->key_length==10 && native_profile_hex(p->key,10));
    if(p->security==2)return p->key_length==13 || (p->key_length==26 && native_profile_hex(p->key,26));
    if(p->security!=3 && p->security!=4)return 0;
    if(p->key_length==64)return native_profile_hex(p->key,64);
    if(p->key_length<8 || p->key_length>63)return 0;
    for(unsigned i=0;i<p->key_length;++i)if(p->key[i]<32 || p->key[i]>126)return 0;
    return 1;
}
/* Encode BEGIN, zero-to-two KEY records, COMMIT; no credential-bearing logging. */
static inline unsigned native_profile_frames(const native_wifi_profile_t *p)
{return 3+(p->key_length+31)/32;}
static inline int native_profile_encode(link_frame_t *f,unsigned seq,unsigned request,
                                       const native_wifi_profile_t *p,unsigned index)
{
    if(!request || !native_profile_valid(p) || index>=native_profile_frames(p))return 0;
    link_init(f,LINK_WIFI_PROFILE,seq,request,NULL);
    f->payload[1]=NATIVE_PROFILE_VERSION;
    if(!index) {
        f->length=40;f->payload[0]=NATIVE_PROFILE_BEGIN;f->payload[2]=p->ssid_length;
        f->payload[3]=p->security;f->payload[4]=p->key_length;f->payload[5]=p->ip_mode;f->payload[6]=p->dns_mode;memcpy(f->payload+8,p->ssid,32);
    } else if(index+1==native_profile_frames(p)){f->length=8;f->payload[0]=NATIVE_PROFILE_COMMIT;}
    else if(index+2==native_profile_frames(p)) {
        f->length=28;f->payload[0]=NATIVE_PROFILE_NETWORK;
        memcpy(f->payload+8,p->ip,4);memcpy(f->payload+12,p->mask,4);memcpy(f->payload+16,p->gateway,4);
        memcpy(f->payload+20,p->dns[0],4);memcpy(f->payload+24,p->dns[1],4);
    } else {
        unsigned offset=(index-1)*32,n=p->key_length-offset;if(n>32)n=32;
        f->length=40;f->payload[0]=NATIVE_PROFILE_KEY;f->payload[2]=offset;f->payload[3]=n;
        memcpy(f->payload+8,p->key+offset,n);
    }
    return 1;
}
/* 1 incomplete, 2 committed, 0 unrelated, -1 invalid (secrets erased).
 * The owner copies a committed profile to its Wi-Fi task then immediately wipes
 * the receiver. Any disconnect, new BEGIN or timeout must also clear it. */
static inline int native_profile_receive(native_profile_receiver_t *r,const link_frame_t *f)
{
    if(f->type!=LINK_WIFI_PROFILE)return 0;
    if(!link_valid(f,sizeof(*f)) || !f->reply_to || f->length<8 || f->payload[1]!=NATIVE_PROFILE_VERSION)goto invalid;
    unsigned op=f->payload[0];
    if(op==NATIVE_PROFILE_BEGIN) {
        native_profile_clear(r,sizeof(*r));
        if(f->length!=40 || f->payload[5]>1 || f->payload[6]>1 || f->payload[7] ||
            !f->payload[2] || f->payload[2]>32 || f->payload[4]>64 ||
            f->payload[3]>4)goto invalid;
        r->request=f->reply_to;r->profile.ssid_length=f->payload[2];r->profile.security=f->payload[3];
        r->profile.key_length=f->payload[4];r->profile.ip_mode=f->payload[5];r->profile.dns_mode=f->payload[6];memcpy(r->profile.ssid,f->payload+8,32);return 1;
    }
    if(f->reply_to!=r->request)return 0;
    if(op==NATIVE_PROFILE_KEY) {
        unsigned offset=f->payload[2],n=f->payload[3];
        if(f->length!=40 || (offset!=0 && offset!=32) || offset>=r->profile.key_length || !n || n>32 ||
           n!=((unsigned)r->profile.key_length-offset>32?32:(unsigned)r->profile.key_length-offset) ||
           link_get_u32(f->payload+4))goto invalid;
        for(unsigned i=n;i<32;++i)if(f->payload[8+i])goto invalid;
        unsigned bit=1u<<(offset/32);
        if((r->keys&bit) && memcmp(r->profile.key+offset,f->payload+8,n))goto invalid;
        memcpy(r->profile.key+offset,f->payload+8,n);r->keys|=bit;return 1;
    }
    if(op==NATIVE_PROFILE_NETWORK) {
        if(f->length!=28 || f->payload[2] || f->payload[3] || link_get_u32(f->payload+4))goto invalid;
        uint8_t previous[20];
        memcpy(previous,r->profile.ip,4);memcpy(previous+4,r->profile.mask,4);memcpy(previous+8,r->profile.gateway,4);
        memcpy(previous+12,r->profile.dns[0],4);memcpy(previous+16,r->profile.dns[1],4);
        memcpy(r->profile.ip,f->payload+8,4);memcpy(r->profile.mask,f->payload+12,4);memcpy(r->profile.gateway,f->payload+16,4);
        memcpy(r->profile.dns[0],f->payload+20,4);memcpy(r->profile.dns[1],f->payload+24,4);
        if(!native_profile_network_valid(&r->profile) || ((r->keys&4) && memcmp(previous,f->payload+8,20)))goto invalid;
        r->keys|=4;return 1;
    }
    if(op==NATIVE_PROFILE_COMMIT) {
        if(f->length!=8 || f->payload[2] || f->payload[3] || link_get_u32(f->payload+4) ||
           r->keys!=(((1u<<((r->profile.key_length+31)/32))-1)|4u) || !native_profile_valid(&r->profile))goto invalid;
        return 2;
    }
invalid:
    native_profile_clear(r,sizeof(*r));return -1;
}
#endif
