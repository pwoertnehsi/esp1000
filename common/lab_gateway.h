#ifndef PSP_LAB_GATEWAY_H
#define PSP_LAB_GATEWAY_H
#include <stddef.h>
#include <stdint.h>
#define VNET_ETH_MAX 1514u
#define VNET_LAB_PORT 31337u
/* Local packet-boundary fixture only. Not the ESP router or production backend. */
extern const uint8_t vnet_lab_psp_ip[4];
extern const uint8_t vnet_lab_gateway_ip[4];
extern const uint8_t vnet_lab_psp_mac[6];
extern const uint8_t vnet_lab_gateway_mac[6];
/* 0: ignore/reject. Positive: complete Ethernet response length. */
size_t vnet_lab_reply(const uint8_t *frame, size_t length, uint8_t *reply, size_t capacity);
uint16_t vnet_checksum(const uint8_t *data, size_t length);
#endif
