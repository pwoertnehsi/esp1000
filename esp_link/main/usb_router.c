/* Custom Ethernet interface for ESP-IDF's bundled lwIP forwarding/NAPT.
 * USB calls remain exclusively in main's host task. TCP/IP callbacks copy to
 * a bounded queue and never wait for USB or retain caller-owned pbufs. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/lwip_napt.h"
#include "netif/ethernet.h"
#include "lab_gateway.h"
#include "usb_router.h"
#include "router_filter.h"
#include "host_wakeup.h"
static struct netif usb_netif;
static QueueHandle_t outgoing;
static volatile bool linked;
static unsigned input_packets, output_packets, dropped;
typedef struct { uint16_t length; uint8_t data[VNET_ETH_MAX]; } router_frame_t;
static err_t output(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    if (!linked || p->tot_len > VNET_ETH_MAX || p->tot_len < 14) return ERR_IF;
    router_frame_t frame;
    frame.length=p->tot_len;
    if (pbuf_copy_partial(p,frame.data,p->tot_len,0)!=p->tot_len) return ERR_BUF;
    if (xQueueSend(outgoing,&frame,0)!=pdTRUE) { ++dropped; return ERR_MEM; }
    host_wakeup_signal(false);
    ++output_packets;
    return ERR_OK;
}
static err_t init(struct netif *n)
{
    n->name[0]='p'; n->name[1]='s';
    n->hwaddr_len=6; memcpy(n->hwaddr,vnet_lab_gateway_mac,6);
    n->mtu=1500; n->flags=NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP|NETIF_FLAG_ETHERNET;
    n->output=etharp_output; n->linkoutput=output;
    return ERR_OK;
}
static void create(void *unused)
{
    (void)unused;
    ip4_addr_t ip,mask,gw;
    IP4_ADDR(&ip,192,168,77,1); IP4_ADDR(&mask,255,255,255,0); ip4_addr_set_zero(&gw);
    configASSERT(netif_add(&usb_netif,&ip,&mask,&gw,NULL,init,tcpip_input));
    netif_set_up(&usb_netif);
    configASSERT(ip_napt_enable_netif(&usb_netif,1));
    /* Wi-Fi retains the default route. This LAN interface is never default. */
}
void usb_router_start(void)
{
    outgoing=xQueueCreate(16,sizeof(router_frame_t)); configASSERT(outgoing);
    ESP_ERROR_CHECK(tcpip_callback_wait(create,NULL)==ERR_OK ? ESP_OK : ESP_FAIL);
    ESP_LOGI("usb-router","Ethernet 192.168.77.1/24, bundled lwIP NAPT enabled");
}
static void link_change(void *enabled)
{
    linked=enabled!=NULL;
    if (linked) { configASSERT(ip_napt_enable_netif(&usb_netif,1)); netif_set_link_up(&usb_netif); }
    else { configASSERT(ip_napt_enable_netif(&usb_netif,0)); netif_set_link_down(&usb_netif); etharp_cleanup_netif(&usb_netif); xQueueReset(outgoing); }
}
void usb_router_link(bool enabled)
{
    /* Serialize behind queued input/output so no old queued frame survives a
     * USB session boundary. Main owns this operation; never call in USB IRQ. */
    ESP_ERROR_CHECK(tcpip_callback_wait(link_change,enabled ? &usb_netif : NULL)==ERR_OK ? ESP_OK : ESP_FAIL);
}
bool usb_router_receive(const void *data,unsigned length)
{
    if (!linked || !usb_router_frame_allowed(data,length)) return false;
    struct pbuf *p=pbuf_alloc(PBUF_RAW,length,PBUF_RAM);
    if (!p) { ++dropped; return false; }
    if (pbuf_take(p,data,length)!=ERR_OK || tcpip_input(p,&usb_netif)!=ERR_OK) {
        pbuf_free(p); ++dropped; return false;
    }
    ++input_packets;
    return true;
}
unsigned usb_router_take(void *data,unsigned capacity)
{
    router_frame_t frame;
    if (xQueueReceive(outgoing,&frame,0)!=pdTRUE) return 0;
    if (frame.length>capacity) { ++dropped; return 0; }
    memcpy(data,frame.data,frame.length); return frame.length;
}
void usb_router_status(void)
{
    ESP_LOGI("usb-router","ROUTER link=%d in=%u out=%u dropped=%u queue=%u",
             linked,input_packets,output_packets,dropped,(unsigned)uxQueueMessagesWaiting(outgoing));
}
