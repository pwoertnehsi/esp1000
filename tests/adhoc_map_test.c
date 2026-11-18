#include "adhoc_map_shim.h"
#include <stdio.h>
#include "../esp_link/main/adhoc_portmap.c"
static void (*timer)(void *);
void sys_timeout(unsigned ms,void (*fn)(void *),void *arg){assert(ms==10000 && !arg);timer=fn;}
void sys_untimeout(void (*fn)(void *),void *arg){assert(!arg);if(timer==fn)timer=NULL;}

static struct netif wifi={{0x4603a8c0}};
struct netif *netif_default=&wifi;
static unsigned added,removed,replies;static int reject_add;static uint8_t last_reply[16];
int ip_portmap_add(unsigned protocol,uint32_t external,unsigned port,uint32_t internal,unsigned target)
{assert((protocol==6 || protocol==17) && external==wifi.ip.addr && port==target && internal==0x024da8c0);++added;return !reject_add;}
int ip_portmap_remove(unsigned protocol,unsigned port){assert((protocol==6 || protocol==17) && port);++removed;return 1;}
unsigned pbuf_copy_partial(struct pbuf *p,void *out,unsigned n,unsigned off){assert(!off && n==16);memcpy(out,p->bytes,n);return n;}
struct pbuf *pbuf_alloc(int type,unsigned n,int ram){(void)type;(void)ram;struct pbuf *p=calloc(1,sizeof(*p));assert(n==16);p->tot_len=n;return p;}
void pbuf_free(struct pbuf *p){free(p);}int pbuf_take(struct pbuf *p,const void *in,unsigned n){memcpy(p->bytes,in,n);return 0;}
struct udp_pcb *udp_new(void){static struct udp_pcb pcb;return &pcb;}
int udp_bind(struct udp_pcb *p,const ip_addr_t *a,unsigned port){assert(p && a->addr==0x014da8c0 && port==27313);return 0;}
void udp_recv(struct udp_pcb *p,void (*cb)(void *,struct udp_pcb *,struct pbuf *,const ip_addr_t *,u16_t),void *arg){assert(p && cb==receive && !arg);}
int udp_sendto(struct udp_pcb *p,struct pbuf *b,const ip_addr_t *a,unsigned port)
{assert(p && a->addr==0x024da8c0 && port==40000);++replies;memcpy(last_reply,b->bytes,16);return 0;}
static void transact(const uint8_t request[16],uint32_t source)
{
    struct pbuf *p=pbuf_alloc(0,16,0);memcpy(p->bytes,request,16);p->address.addr=source;
    /* Address deliberately belongs to freed request storage: response needs copy. */
    receive(NULL,service,p,&p->address,40000);
}
int main(void)
{
    adhoc_portmap_start();adhoc_portmap_resume();uint8_t packet[16],bad[16];int32_t status;
    adhoc_map_request(packet,1,17,13090,7);assert(adhoc_map_valid(packet,16));
    assert(!adhoc_map_valid(packet,15));memcpy(bad,packet,16);bad[0]^=1;assert(!adhoc_map_valid(bad,16));
    memcpy(bad,packet,16);bad[5]=1;assert(!adhoc_map_valid(bad,16));
    memcpy(bad,packet,16);bad[8]=0;assert(!adhoc_map_valid(bad,16));
    transact(packet,0x0100007f);assert(!added && !replies);
    transact(packet,0x024da8c0);assert(added==1 && replies==1 && adhoc_map_reply(packet,last_reply,16,&status) && !status);
    memcpy(bad,last_reply,16);bad[8]^=1;assert(!adhoc_map_reply(packet,bad,16,&status));
    transact(packet,0x024da8c0);assert(added==1); /* Idempotent request. */
    for(unsigned i=1;i<16;++i){adhoc_map_request(packet,1,6,13090+i,i+10);assert(!change(packet));}
    adhoc_map_request(packet,1,17,14000,30);assert(change(packet)<0);
    adhoc_map_request(packet,2,17,13090,31);assert(!change(packet) && removed==1);
    reject_add=1;adhoc_map_request(packet,1,17,14000,32);assert(change(packet)<0);reject_add=0;assert(!change(packet));
    adhoc_map_request(packet,3,0,0,33);assert(adhoc_map_valid(packet,16));assert(!change(packet) && removed==17);
    for(unsigned i=0;i<16;++i)assert(!mappings[i].port);
    adhoc_map_request(packet,1,17,10001,50);assert(!change(packet));
    unsigned before=added;
    for(unsigned i=0;i<1000;++i)assert(!change(packet));
    assert(added==before);
    adhoc_portmap_pause();assert(paused && timer && mappings[0].port==10001 && !mappings[0].address);
    adhoc_portmap_pause(); /* repeated down cannot extend the window */
    assert(change(packet)<0); /* no active forward while detached */
    adhoc_portmap_resume();assert(!paused && !timer && added==before+1 && mappings[0].address);
    assert(!change(packet) && added==before+1);
    before=added;wifi.ip.addr++;assert(!change(packet) && added==before+1);
    adhoc_portmap_pause();assert(timer);timer(NULL);assert(!mappings[0].port);
    before=added;adhoc_portmap_resume();assert(added==before);
    assert(!change(packet));adhoc_portmap_pause();adhoc_portmap_clear();adhoc_portmap_resume();
    assert(!mappings[0].port);
    puts("PASS: real ESP mapping service source isolation, wire validation, ACK correlation, address lifetime, capacity, removal and clearing");
}
