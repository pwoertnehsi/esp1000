#include "lab_gateway.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void w16(unsigned char *p,unsigned n) { p[0]=n>>8;p[1]=n; }
static unsigned r16(const unsigned char *p) { return p[0]*256u+p[1]; }
static unsigned ref_sum(const unsigned char *p,size_t n)
{
    unsigned sum=0;
    for(size_t i=0;i<n;i++) sum+=(unsigned)p[i] << (i%2 ? 0 : 8);
    while(sum>>16)sum=(sum&65535u)+(sum>>16);
    return sum;
}
static unsigned ref_udp(const unsigned char *ip,const unsigned char *udp,size_t n)
{
    unsigned sum=ref_sum(ip+12,8)+17+(unsigned)n+ref_sum(udp,n);
    while(sum>>16)sum=(sum&65535u)+(sum>>16);
    return (~sum)&65535u;
}
static unsigned char input[VNET_ETH_MAX],reply[VNET_ETH_MAX],saved[VNET_ETH_MAX];
static size_t arp(void)
{
    memset(input,0,sizeof(input));memset(input,255,6);
    memcpy(input+6,vnet_lab_psp_mac,6);w16(input+12,0x806);
    w16(input+14,1);w16(input+16,0x800);input[18]=6;input[19]=4;w16(input+20,1);
    memcpy(input+22,vnet_lab_psp_mac,6);memcpy(input+28,vnet_lab_psp_ip,4);
    memcpy(input+38,vnet_lab_gateway_ip,4);return 42;
}
static size_t udp(size_t payload,int checksum)
{
    memset(input,0,sizeof(input));memcpy(input,vnet_lab_gateway_mac,6);memcpy(input+6,vnet_lab_psp_mac,6);w16(input+12,0x800);
    unsigned char *ip=input+14,*u=ip+20;
    ip[0]=0x45;ip[8]=64;ip[9]=17;w16(ip+2,28+payload);
    memcpy(ip+12,vnet_lab_psp_ip,4);memcpy(ip+16,vnet_lab_gateway_ip,4);
    w16(ip+10,~ref_sum(ip,20));w16(u,40000);w16(u+2,VNET_LAB_PORT);w16(u+4,8+payload);
    for(size_t i=0;i<payload;i++)u[8+i]=(unsigned char)(i*17u+3);
    if(checksum){unsigned c=ref_udp(ip,u,8+payload);w16(u+6,c?c:65535);}
    return 42+payload;
}
int main(void)
{
    static const unsigned char vector[]={0,1,0xf2,3,0xf4,0xf5,0xf6,0xf7};
    static const unsigned char odd[]={1,2,3};
    assert(vnet_checksum(vector,sizeof(vector))==0x220d);
    assert(vnet_checksum(odd,sizeof(odd))==0xfbfd);
    size_t n=arp();assert(vnet_lab_reply(input,n,reply,sizeof(reply))==42);
    assert(!memcmp(reply,vnet_lab_psp_mac,6)&&!memcmp(reply+6,vnet_lab_gateway_mac,6));
    assert(r16(reply+20)==2&&!memcmp(reply+28,vnet_lab_gateway_ip,4)&&!memcmp(reply+38,vnet_lab_psp_ip,4));
    assert(vnet_lab_reply(input,n,reply,41)==0);
    memcpy(saved,input,n);
    unsigned offsets[]={14,16,18,19,20,22,28,38};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){
        memcpy(input,saved,n);input[offsets[i]]^=1;assert(vnet_lab_reply(input,n,reply,sizeof(reply))==0);
    }
    static const unsigned sizes[]={0,1,25,191,192,208,209,511,1472};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)for(int checksum=0;checksum<2;checksum++){
        n=udp(sizes[i],checksum);size_t got=vnet_lab_reply(input,n,reply,sizeof(reply));assert(got==n);
        assert(ref_sum(reply+14,20)==65535 && ref_udp(reply+14,reply+34,8+sizes[i])==0);
        assert(r16(reply+34)==VNET_LAB_PORT&&r16(reply+36)==40000);
        assert(!memcmp(input+42,reply+42,sizes[i]));
        assert(vnet_lab_reply(input,n,reply,n-1)==0);
        if(checksum){input[n-1]^=1;assert(vnet_lab_reply(input,n,reply,sizeof(reply))==0);}
    }
    n=udp(25,1);memcpy(saved,input,n);
    unsigned bad_offsets[]={0,6,14,16,20,22,23,24,26,30,36,38};
    for(unsigned i=0;i<sizeof(bad_offsets)/sizeof(bad_offsets[0]);i++){
        memcpy(input,saved,n);input[bad_offsets[i]]^=1;assert(vnet_lab_reply(input,n,reply,sizeof(reply))==0);
    }
    unsigned random=7;
    for(unsigned trial=0;trial<100000;trial++){
        for(unsigned i=0;i<sizeof(input);i++){random=random*1664525u+1013904223u;input[i]=random>>24;}
        memcpy(input+6,vnet_lab_psp_mac,6);input[12]=8;input[13]=trial&1?6:0;
        random=random*1664525u+1013904223u;n=random%(sizeof(input)+1);
        size_t cap=(random>>12)%(sizeof(reply)+1);
        assert(vnet_lab_reply(input,n,reply,cap)<=cap);
    }
    puts("PASS: local ARP/UDP fixture, checksums, binary payloads, 100000 malformed frames");
    return 0;
}
