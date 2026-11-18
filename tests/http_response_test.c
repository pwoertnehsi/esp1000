#include "http_response.h"
#include "native_route.h"
#include "router_filter.h"
#include <assert.h>
#include <stdio.h>
static unsigned seed=1;
static unsigned random_byte(void) {seed=seed*1664525+1013904223;return seed>>24;}
int main(void)
{
    size_t offset=0,length=0;
    static const char good[]="HTTP/1.0 200 OK\r\nContent-Length: 5\r\n\r\nHello";
    assert(http_response_body((void *)good,strlen(good),&offset,&length) && length==5 && !memcmp(good+offset,"Hello",5));
    for(unsigned n=0;n<strlen(good);++n) assert(!http_response_body((void *)good,n,&offset,&length));
    static const char close[]="HTTP/1.1 200 OK\r\nServer: test\r\n\r\nHello";
    assert(http_response_body((void *)close,strlen(close),&offset,&length) && length==5);
    const char *bad[]={"HTTP/1.0 200 OK\r\nContent-Encoding: gzip\r\n\r\nHello","HTTP/1.0 301 Redirect\r\n\r\nhello","HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nHello","HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nHello",
      "HTTP/1.0 200 OK\r\nContent-Length: 99999999999999999999999\r\n\r\nHello",
      "HTTP/1.0 200 OK\r\n folded\r\n\r\nhello","HTTP/1.0 200 OK\r\n\r\n"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(!http_response_body((void *)bad[i],strlen(bad[i]),&offset,&length));
    uint8_t ethernet[42]={0};memcpy(ethernet+6,vnet_lab_psp_mac,6);
    ethernet[12]=8;ethernet[13]=6;ethernet[15]=1;ethernet[16]=8;ethernet[18]=6;ethernet[19]=4;ethernet[21]=1;
    memcpy(ethernet+22,vnet_lab_psp_mac,6);memcpy(ethernet+28,vnet_lab_psp_ip,4);
    assert(usb_router_frame_allowed(ethernet,42));
    ethernet[22]^=1;assert(!usb_router_frame_allowed(ethernet,42));ethernet[22]^=1;
    ethernet[13]=0;ethernet[14]=0x45;ethernet[16]=0;ethernet[17]=20;memcpy(ethernet+26,vnet_lab_psp_ip,4);
    assert(usb_router_frame_allowed(ethernet,34));
    ethernet[17]=30;assert(!usb_router_frame_allowed(ethernet,34));
    uint8_t fuzz[512];
    for(unsigned i=0;i<100000;++i) {
        unsigned n=random_byte()*2;for(unsigned j=0;j<n;++j)fuzz[j]=random_byte();
        if(n>=13 && (i&1))memcpy(fuzz,"HTTP/1.0 200 ",13);
        assert(!http_response_body(fuzz,n,&offset,&length));
        assert(!usb_router_frame_allowed(fuzz,n));
    }
    uint8_t route[VNET_ROUTE_SIZE],expected[VNET_ROUTE_SIZE]={0};
    expected[0]=124;expected[2]=3;expected[3]=1;expected[8]=3;expected[12]=7;
    expected[20]=1;expected[21]=0x46;expected[22]=0x50;expected[23]=0x57;
    expected[76]=expected[92]=expected[108]=16;expected[77]=expected[93]=2;
    expected[96]=192;expected[97]=168;expected[98]=77;expected[99]=1;
    vnet_default_route(route);assert(!memcmp(route,expected,sizeof(route)));
    puts("PASS: exact 6.61 route message, HTTP framing/truncation/overflow and 100k malformed responses");
}
