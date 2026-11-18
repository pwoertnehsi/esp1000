/* LAN-only port mappings for the ESP-backed aemu client; TCP/IP task owns state. */
#include "freertos/FreeRTOS.h"
#include "adhoc_portmap.h"
#include "lwip/udp.h"
#include "lwip/lwip_napt.h"
#include "lwip/netif.h"
#include "lab_gateway.h"
#include "esp_log.h"
static struct {uint16_t port;uint8_t protocol;} mappings[16];
static struct udp_pcb *service;
void adhoc_portmap_clear(void)
{
    for(unsigned i=0;i<16;++i)if(mappings[i].port) {
        ip_portmap_remove(mappings[i].protocol,mappings[i].port);mappings[i].port=0;
    }
}
static int change(const uint8_t *p)
{
    unsigned port=p[6]|(unsigned)p[7]<<8,proto=p[5];int slot=-1;
    if(p[4]==ADHOC_PORTMAP_CLEAR){adhoc_portmap_clear();return 0;}
    for(unsigned i=0;i<16;++i)if(mappings[i].port==port && mappings[i].protocol==proto){slot=i;break;}
    if(p[4]==ADHOC_PORTMAP_REMOVE) {
        if(slot>=0){ip_portmap_remove(proto,port);mappings[slot].port=0;}return 0;
    }
    if(slot<0)for(unsigned i=0;i<16;++i)if(!mappings[i].port){slot=i;break;}
    if(slot<0 || !netif_default || ip4_addr_isany_val(*netif_ip4_addr(netif_default)))return -1;
    ip4_addr_t target;IP4_ADDR(&target,192,168,77,2);
    if(!ip_portmap_add(proto,ip4_addr_get_u32(netif_ip4_addr(netif_default)),port,ip4_addr_get_u32(&target),port))return -2;
    mappings[slot].port=port;mappings[slot].protocol=proto;return 0;
}
static void receive(void *arg,struct udp_pcb *pcb,struct pbuf *packet,const ip_addr_t *address,u16_t port)
{
    (void)arg;uint8_t bytes[16];ip_addr_t peer;IP_ADDR4(&peer,192,168,77,2);
    if(!ip_addr_cmp(address,&peer) || packet->tot_len!=16 || pbuf_copy_partial(packet,bytes,16,0)!=16 || !adhoc_map_valid(bytes,16)){pbuf_free(packet);return;}
    ip_addr_t reply_address=*address;
    int status=change(bytes);bytes[4]|=0x80;adhoc_map_put(bytes+12,(uint32_t)status);pbuf_free(packet);
    struct pbuf *reply=pbuf_alloc(PBUF_TRANSPORT,16,PBUF_RAM);
    if(reply){pbuf_take(reply,bytes,16);udp_sendto(pcb,reply,&reply_address,port);pbuf_free(reply);}
    ESP_LOGI("adhoc-map","op=%u proto=%u port=%u result=%d",bytes[4]&0x7f,bytes[5],bytes[6]|bytes[7]<<8,status);
}
void adhoc_portmap_start(void)
{
    ip_addr_t local;IP_ADDR4(&local,192,168,77,1);
    service=udp_new();configASSERT(service);
    configASSERT(udp_bind(service,&local,ADHOC_PORTMAP_PORT)==ERR_OK);udp_recv(service,receive,NULL);
}
