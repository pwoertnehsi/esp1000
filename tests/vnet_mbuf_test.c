#include "../virtual_net/driver/mbuf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE 0x1000u
static unsigned char pool[32][256];
static unsigned allocated[32],gets,frees,fail_at,writes,fail_write_at;
static int access_address(uint32_t a,size_t n)
{
    if(a<BASE||n>sizeof(pool)||a-BASE>sizeof(pool)-n)return 0;
    if(n && (a-BASE)/256!=(a-BASE+n-1)/256)return 0;
    return allocated[(a-BASE)/256];
}
static int read_pool(void *c,uint32_t a,void *out,size_t n)
{ (void)c;if(!access_address(a,n))return 0;memcpy(out,(unsigned char *)pool+a-BASE,n);return 1; }
static int write_pool(void *c,uint32_t a,const void *p,size_t n)
{ (void)c;if(++writes==fail_write_at || !access_address(a,n))return 0;memcpy((unsigned char *)pool+a-BASE,p,n);return 1; }
static uint32_t get(void *c)
{
    (void)c;gets++;if(gets==fail_at)return 0;
    for(unsigned i=0;i<32;i++)if(!allocated[i]){allocated[i]=1;memset(pool[i],0,256);return BASE+i*256;}
    return 0;
}
static void free_chain(void *c,uint32_t a)
{
    (void)c;unsigned count=0;
    while(a){assert(++count<33);assert(a>=BASE && (a-BASE)%256==0);unsigned i=(a-BASE)/256;assert(i<32&&allocated[i]);uint32_t next;memcpy(&next,pool[i],4);allocated[i]=0;frees++;a=next;}
}
static const vnet_mbuf_ops_t ops={NULL,read_pool,write_pool,get,free_chain};
static void reset(void){memset(allocated,0,sizeof(allocated));gets=frees=fail_at=writes=fail_write_at=0;}
static unsigned char input[VNET_MBUF_MAX],output[VNET_MBUF_MAX];
int main(void)
{
    for(unsigned i=0;i<sizeof(input);i++)input[i]=(unsigned char)(i*37u+3);
    static const unsigned sizes[]={14,42,60,63,64,65,128,206,207,208,209,236,256,512,1500,1514};
    for(unsigned repeat=0;repeat<100;repeat++)for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
        reset();unsigned n=sizes[i];uint32_t head=vnet_mbuf_make(&ops,input,n,0x1234);assert(head);
        uint32_t first_data;assert(read_pool(NULL,head+8,&first_data,4));
        assert(first_data==head+50 && (first_data+14)%4==0);
        uint32_t first_length;assert(read_pool(NULL,head+12,&first_length,4));
        assert(first_length==(n<206?n:206));
        size_t got=0;assert(vnet_mbuf_copy(&ops,head,output,sizeof(output),&got)&&got==n&&!memcmp(input,output,n));
        assert(!vnet_mbuf_copy(&ops,head,output,n-1,&got));
        free_chain(NULL,head);assert(gets==frees);
    }
    for(unsigned failure=1;failure<=7;failure++){
        reset();fail_at=failure;assert(!vnet_mbuf_make(&ops,input,1514,0x1234));assert(frees==gets-1);
        for(unsigned i=0;i<32;i++)assert(!allocated[i]);
    }
    for(unsigned failure=1;failure<=21;failure++){
        reset();fail_write_at=failure;assert(!vnet_mbuf_make(&ops,input,1514,0x1234));assert(frees==gets);
        for(unsigned i=0;i<32;i++)assert(!allocated[i]);
    }
    reset();assert(!vnet_mbuf_make(&ops,input,13,0));assert(!vnet_mbuf_make(&ops,input,1515,0));assert(!gets);
    uint32_t head=vnet_mbuf_make(&ops,input,64,0x1234);assert(head);size_t got;
    uint32_t negative=0xffffffff;write_pool(NULL,head+12,&negative,4);assert(!vnet_mbuf_copy(&ops,head,output,sizeof(output),&got));
    unsigned n=64;write_pool(NULL,head+12,&n,4);
    uint32_t bad_total=65;write_pool(NULL,head+24,&bad_total,4);assert(!vnet_mbuf_copy(&ops,head,output,sizeof(output),&got));
    write_pool(NULL,head+24,&n,4);write_pool(NULL,head,&head,4);assert(!vnet_mbuf_copy(&ops,head,output,sizeof(output),&got));
    uint32_t zero=0;write_pool(NULL,head,&zero,4);free_chain(NULL,head);
    puts("PASS: native mbuf layout, 1600 binary round trips, allocation failures and malformed chains");
    return 0;
}
