#ifndef ADHOC_MAP_SHIM_H
#define ADHOC_MAP_SHIM_H
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef uint16_t u16_t;typedef uint8_t u8_t;typedef uint32_t u32_t;
typedef struct {uint32_t addr;} ip_addr_t;
typedef ip_addr_t ip4_addr_t;
struct netif {ip4_addr_t ip;};extern struct netif *netif_default;
#define IP_ADDR4(p,a,b,c,d) ((p)->addr=(a)|((uint32_t)(b)<<8)|((uint32_t)(c)<<16)|((uint32_t)(d)<<24))
#define IP4_ADDR IP_ADDR4
#define ip_addr_cmp(a,b) ((a)->addr==(b)->addr)
#define netif_ip4_addr(n) (&(n)->ip)
#define ip4_addr_isany_val(p) (!(p).addr)
#define ip4_addr_get_u32(p) ((p)->addr)
#define configASSERT assert
static inline void mock_log(const char *tag,const char *format,...){(void)tag;(void)format;}
#define ESP_LOGI mock_log
#define PBUF_TRANSPORT 0
#define PBUF_RAM 0
#define ERR_OK 0
struct udp_pcb {int unused;};
struct pbuf {unsigned tot_len;unsigned char bytes[16];ip_addr_t address;};
unsigned pbuf_copy_partial(struct pbuf *,void *,unsigned,unsigned);
struct pbuf *pbuf_alloc(int,unsigned,int);void pbuf_free(struct pbuf *);int pbuf_take(struct pbuf *,const void *,unsigned);
struct udp_pcb *udp_new(void);int udp_bind(struct udp_pcb *,const ip_addr_t *,unsigned);
void udp_recv(struct udp_pcb *,void (*)(void *,struct udp_pcb *,struct pbuf *,const ip_addr_t *,u16_t),void *);
int udp_sendto(struct udp_pcb *,struct pbuf *,const ip_addr_t *,unsigned);
int ip_portmap_add(unsigned,uint32_t,unsigned,uint32_t,unsigned);int ip_portmap_remove(unsigned,unsigned);
void sys_timeout(unsigned,void (*)(void *),void *);
void sys_untimeout(void (*)(void *),void *);
#endif
