/* LAN-only port mappings for the ESP-backed aemu client; TCP/IP task owns state. */
#include "freertos/FreeRTOS.h"
#include "adhoc_portmap.h"
#include "lwip/udp.h"
#include "lwip/timeouts.h"
#include "lwip/lwip_napt.h"
#include "lwip/netif.h"
#include "lab_gateway.h"
#include "esp_log.h"
static struct {uint16_t port;uint8_t protocol;uint32_t address;} mappings[16];
static int paused=1;
static struct udp_pcb *service;
static void expire(void *unused);
static void remove_installed(void)
{
    for(unsigned i=0;i<16;++i)if(mappings[i].port && mappings[i].address) {
        ip_portmap_remove(mappings[i].protocol,mappings[i].port);
        mappings[i].address=0;
    }
}
void adhoc_portmap_clear(void)
{
    sys_untimeout(expire,NULL);
    remove_installed();
    for(unsigned i=0;i<16;++i)mappings[i].port=0;
}
static void expire(void *unused)
{
    (void)unused;
    if(paused)adhoc_portmap_clear();
}
static int install(unsigned slot)
{
    if(paused || !netif_default || ip4_addr_isany_val(*netif_ip4_addr(netif_default)))return -1;
    uint32_t address=ip4_addr_get_u32(netif_ip4_addr(netif_default));
    if(mappings[slot].address==address)return 0;
    if(mappings[slot].address)ip_portmap_remove(mappings[slot].protocol,mappings[slot].port);
    mappings[slot].address=0;
    ip4_addr_t target;IP4_ADDR(&target,192,168,77,2);
    if(!ip_portmap_add(mappings[slot].protocol,address,mappings[slot].port,ip4_addr_get_u32(&target),mappings[slot].port))return -2;
    mappings[slot].address=address;return 0;
}
/* Call before disabling NAPT: that frees lwIP's entire mapping table. Retain
 * only intent, for a bounded cable-recovery window, never an active forward. */
void adhoc_portmap_pause(void)
{
    remove_installed();
    if(!paused){paused=1;sys_timeout(10000,expire,NULL);}
}
/* Call after NAPT has been re-enabled, before accepting new USB frames. */
void adhoc_portmap_resume(void)
{
    sys_untimeout(expire,NULL);paused=0;
    for(unsigned i=0;i<16;++i)if(mappings[i].port) {
        int result=install(i);
        ESP_LOGI("adhoc-map","restore proto=%u port=%u result=%d",mappings[i].protocol,mappings[i].port,result);
    }
}
static int change(const uint8_t *p)
{
    unsigned port=p[6]|(unsigned)p[7]<<8,proto=p[5];int slot=-1;
    if(p[4]==ADHOC_PORTMAP_CLEAR){adhoc_portmap_clear();return 0;}
    for(unsigned i=0;i<16;++i)if(mappings[i].port==port && mappings[i].protocol==proto){slot=i;break;}
    if(p[4]==ADHOC_PORTMAP_REMOVE) {
        if(slot>=0){if(mappings[slot].address)ip_portmap_remove(proto,port);mappings[slot].port=0;mappings[slot].address=0;}return 0;
    }
    if(slot<0)for(unsigned i=0;i<16;++i)if(!mappings[i].port){slot=i;break;}
    if(slot<0)return -1;
    int fresh=!mappings[slot].port;
    mappings[slot].port=port;mappings[slot].protocol=proto;
    int result=install(slot);
    if(result && fresh)mappings[slot].port=0;
    return result;
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
