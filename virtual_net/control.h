#ifndef PSP_VNET_CONTROL_H
#define PSP_VNET_CONTROL_H
#include <stdint.h>
#ifdef VNET_USB_LAB
#define VNET_START_MAGIC 0x55533901u
#else
#define VNET_START_MAGIC 0x564e3901u
#endif
#define VNET_NETTERM_MAGIC 0x564e3902u
#define VNET_REGISTERED 1u
#define VNET_CALLBACK_OK 2u
#define VNET_LAB_BACKEND 4u
#define VNET_USB_READY 8u
#define VNET_UPLINK_READY 16u
#define VNET_REMOTE_READY 32u
#define VNET_WIFI_INFO_READY 64u
#define VNET_STOP_UNSAFE (-2)
/* StartModule copies its arguments, so pass an explicit user result address. */
typedef struct {
    uint32_t magic;
    uint32_t result_address;
} vnet_start_args_t;
typedef struct {
    int32_t result;
    uint32_t flags;
    uint8_t remote_ip[4];
    uint8_t wifi_info[48];
    uint32_t revision;
} vnet_start_result_t;
/* Seqlock snapshot for metadata published by the kernel packet worker. */
static inline int vnet_status_snapshot(vnet_start_result_t *out,const volatile vnet_start_result_t *in)
{
    if(!in)return 0;
    for(unsigned attempt=0;attempt<3;++attempt) {
        unsigned before=in->revision;if(before&1)continue;
        __sync_synchronize();
        for(unsigned i=0;i<sizeof(*out);++i)((unsigned char *)out)[i]=((const volatile unsigned char *)in)[i];
        __sync_synchronize();
        if(before==in->revision)return 1;
    }
    return 0;
}
#endif
