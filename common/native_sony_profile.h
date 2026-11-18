#ifndef ESP_NATIVE_SONY_PROFILE_H
#define ESP_NATIVE_SONY_PROFILE_H
#include "native_wifi_profile.h"
/* 6.61 utility's registry descriptor table at ac20, verified from owned module:
 * auth_proto0=open,1=WEP40,2=WEP104,3=WPA-TKIP,4=WPA-AES;
 *21=wpa_key_type (0=32 raw PSK bytes,1=passphrase),22=wpa_key64 bytes.
 * The netconf parser at c4c converts64hex to32raw bytes, or copies8..63ASCII. */
typedef union {uint32_t number;uint8_t bytes[128];} native_netparam_t;
typedef int (*native_netparam_getter)(int,int,void *);
static inline unsigned native_bounded_length(const uint8_t *p,unsigned limit)
{unsigned n=0;while(n<limit && p[n])++n;return n;}
static inline int native_profile_parse_ip(const uint8_t *text,uint8_t ip[4])
{
    unsigned at=0;
    for(unsigned i=0;i<4;++i) {
        unsigned n=0,digits=0;
        while(at<16 && text[at]>='0' && text[at]<='9') {
            n=n*10+text[at++]-'0';if(++digits>3 || n>255)return 0;
        }
        if(!digits || at>=16 || text[at]!=(i==3?0:'.'))return 0;
        ip[i]=n;++at;
    }
    return 1;
}
static inline int native_sony_profile_read(int index,native_netparam_getter get,native_wifi_profile_t *profile)
{
    native_netparam_t value;memset(profile,0,sizeof(*profile));memset(&value,0,sizeof(value));
    if(index<0 || index>32 || get(index,1,&value))goto fail;
    unsigned n=native_bounded_length(value.bytes,33);if(!n || n>32)goto fail;
    profile->ssid_length=n;memcpy(profile->ssid,value.bytes,n);
    memset(&value,0,sizeof(value));if(get(index,2,&value) || value.number>4)goto fail;
    profile->security=value.number;
    if(profile->security==1 || profile->security==2) {
        unsigned bytes=profile->security==1?5:13;
        memset(&value,0,sizeof(value));if(get(index,3,&value))goto fail;
        static const char hex[]="0123456789abcdef";
        for(unsigned i=0;i<bytes;++i){profile->key[2*i]=hex[value.bytes[i]>>4];profile->key[2*i+1]=hex[value.bytes[i]&15];}
        profile->key_length=2*bytes;
    } else if(profile->security>=3) {
        memset(&value,0,sizeof(value));if(get(index,21,&value) || value.number>1)goto fail;
        unsigned type=value.number;
        memset(&value,0,sizeof(value));if(get(index,22,&value))goto fail;
        if(type) {
            n=native_bounded_length(value.bytes,64);if(n<8 || n>63)goto fail;
            profile->key_length=n;memcpy(profile->key,value.bytes,n);
        } else {
            static const char hex[]="0123456789abcdef";
            for(unsigned i=0;i<32;++i){profile->key[2*i]=hex[value.bytes[i]>>4];profile->key[2*i+1]=hex[value.bytes[i]&15];}
            profile->key_length=64;
        }
    }
    memset(&value,0,sizeof(value));if(get(index,4,&value) || value.number>1)goto fail;
    profile->ip_mode=value.number;
    memset(&value,0,sizeof(value));if(get(index,8,&value) || value.number>1)goto fail;
    profile->dns_mode=value.number;
    if(profile->ip_mode)for(unsigned i=0;i<3;++i) {
        memset(&value,0,sizeof(value));
        uint8_t *destination=i==0?profile->ip:i==1?profile->mask:profile->gateway;
        if(get(index,5+i,&value) || !native_profile_parse_ip(value.bytes,destination))goto fail;
    }
    if(profile->dns_mode)for(unsigned i=0;i<2;++i) {
        memset(&value,0,sizeof(value));if(get(index,9+i,&value))goto fail;
        if(!value.bytes[0] && i==1)continue;
        if(!native_profile_parse_ip(value.bytes,profile->dns[i]))goto fail;
    }
    native_profile_clear(&value,sizeof(value));
    if(native_profile_valid(profile))return 0;
fail:
    native_profile_clear(&value,sizeof(value));native_profile_clear(profile,sizeof(*profile));return -1;
}
#endif
