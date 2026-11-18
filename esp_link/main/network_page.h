#ifndef PSP_NETWORK_PAGE_H
#define PSP_NETWORK_PAGE_H
#include <stdint.h>
#include <stdbool.h>
#include "native_wifi_profile.h"

#define PAGE_URL "https://pspdev.github.io/"
typedef struct {
    uint8_t *data;
    uint32_t size, crc;
} network_page_t;
void network_page_start(void);
int network_page_apply_profile(const native_wifi_profile_t *profile);
void network_page_request(void);
unsigned network_page_uplink(uint8_t remote_ip[4]);
void network_page_wifi_info(uint8_t output[48]);
bool network_page_take(network_page_t *page);
void network_page_status(void);
#endif
