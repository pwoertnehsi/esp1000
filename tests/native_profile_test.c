#include <assert.h>
#include <stdio.h>
#include "../common/native_sony_profile.h"
static unsigned security=4,key_type=1,network_mode;
static int get(int id,int field,void *data)
{
    assert(id==0 || id==3);native_netparam_t *v=data;memset(v,0,sizeof(*v));
    switch(field){case 1:memcpy(v->bytes,"test-ap",7);break;case 2:v->number=security;break;
    case 4:case 8:v->number=network_mode;break;
    case 5:memcpy(v->bytes,"192.168.1.20",13);break;
    case 6:memcpy(v->bytes,"255.255.255.0",14);break;
    case 7:memcpy(v->bytes,"192.168.1.1",12);break;
    case 9:memcpy(v->bytes,"1.1.1.1",8);break;
    case 10:memcpy(v->bytes,"8.8.8.8",8);break;
    case 3:for(unsigned i=0;i<13;++i)v->bytes[i]=i;break;
    case 21:v->number=key_type;break;case 22:if(key_type)memcpy(v->bytes,"test-passphrase",15);else for(unsigned i=0;i<32;++i)v->bytes[i]=i;break;
    default:assert(0);}
    return 0;
}
int main(void)
{
    native_wifi_profile_t profile;native_profile_receiver_t receiver={0};link_frame_t frame;
    for(security=0;security<=4;++security)for(key_type=0;key_type<=1;++key_type) {
        assert(!native_sony_profile_read(3,get,&profile));assert(native_profile_valid(&profile));
        assert(profile.security==security && profile.ssid_length==7);
        unsigned frames=native_profile_frames(&profile);
        for(unsigned i=0;i<frames;++i) {
            assert(native_profile_encode(&frame,i+1,19,&profile,i));
            int r=native_profile_receive(&receiver,&frame);assert(r==(i+1==frames?2:1));
            if(i+1!=frames)assert(native_profile_receive(&receiver,&frame)==1);
        }
        assert(!memcmp(&receiver.profile,&profile,sizeof(profile)));
        native_profile_clear(&receiver,sizeof(receiver));
    }
    security=4;key_type=0;assert(!native_sony_profile_read(0,get,&profile));assert(profile.key_length==64);
    assert(native_profile_encode(&frame,1,19,&profile,0));assert(native_profile_receive(&receiver,&frame)==1);
    assert(native_profile_encode(&frame,2,19,&profile,native_profile_frames(&profile)-1));assert(native_profile_receive(&receiver,&frame)<0 && !receiver.request);
    for(unsigned i=0;i<sizeof(receiver);++i)assert(!((uint8_t *)&receiver)[i]);
    assert(native_profile_encode(&frame,1,19,&profile,0));assert(native_profile_receive(&receiver,&frame)==1);
    assert(native_profile_encode(&frame,2,19,&profile,1));assert(native_profile_receive(&receiver,&frame)==1);
    frame.payload[8]^=1;assert(native_profile_receive(&receiver,&frame)<0 && !receiver.request);
    profile.key[0]='!';assert(!native_profile_valid(&profile));
    network_mode=1;security=4;key_type=1;
    assert(!native_sony_profile_read(3,get,&profile) && profile.ip_mode && profile.dns_mode && profile.ip[3]==20 && profile.dns[0][0]==1);
    for(unsigned i=0;i<native_profile_frames(&profile);++i) {
        assert(native_profile_encode(&frame,i+1,20,&profile,i));
        assert(native_profile_receive(&receiver,&frame)==(i+1==native_profile_frames(&profile)?2:1));
    }
    assert(!memcmp(&receiver.profile,&profile,sizeof(profile)));
    profile.mask[1]=0xdf;assert(!native_profile_valid(&profile));
    uint8_t parsed[4];assert(!native_profile_parse_ip((const uint8_t *)"256.1.1.1",parsed));
    assert(!native_profile_parse_ip((const uint8_t *)"1.1.1",parsed));
    assert(!native_profile_parse_ip((const uint8_t *)"1.1.1.1x",parsed));
    security=5;assert(native_sony_profile_read(3,get,&profile)<0);
    for(unsigned i=0;i<sizeof(profile);++i)assert(!((uint8_t *)&profile)[i]);
    puts("PASS: native saved profiles, WEP binary/hex, WPA passphrase/raw PSK, atomic wire commit and secret clearing");
}
