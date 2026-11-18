#include "lab_gateway.h"
#include <string.h>

const uint8_t vnet_lab_psp_ip[4] = {192, 168, 77, 2};
const uint8_t vnet_lab_gateway_ip[4] = {192, 168, 77, 1};
const uint8_t vnet_lab_psp_mac[6] = {2, 0x50, 0x53, 0x50, 0, 1};
const uint8_t vnet_lab_gateway_mac[6] = {2, 0x50, 0x53, 0x50, 0, 0xfe};

static uint16_t read16(const uint8_t *p)
{ return (uint16_t)((unsigned)p[0] << 8 | p[1]); }
static void write16(uint8_t *p, uint16_t value)
{ p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value; }
static uint32_t sum_bytes(uint32_t sum, const uint8_t *p, size_t length)
{
    while (length >= 2) { sum += read16(p); p += 2; length -= 2; }
    if (length) sum += (unsigned)*p << 8;
    return sum;
}
static uint16_t finish_sum(uint32_t sum)
{
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return (uint16_t)~sum;
}
uint16_t vnet_checksum(const uint8_t *data, size_t length)
{ return finish_sum(sum_bytes(0, data, length)); }
static uint16_t udp_checksum(const uint8_t *ip, const uint8_t *udp, size_t length)
{
    uint32_t sum = sum_bytes(0, ip + 12, 8) + 17u + (uint32_t)length;
    return finish_sum(sum_bytes(sum, udp, length));
}

size_t vnet_lab_reply(const uint8_t *frame, size_t length, uint8_t *reply, size_t capacity)
{
    if (length < 14 || length > VNET_ETH_MAX
        || memcmp(frame + 6, vnet_lab_psp_mac, 6)) return 0;
    unsigned type = read16(frame + 12);
    if (type == 0x0806) {
        if (length < 42 || capacity < 42) return 0;
        const uint8_t *arp = frame + 14;
        if (read16(arp) != 1 || read16(arp + 2) != 0x0800 || arp[4] != 6 || arp[5] != 4
            || read16(arp + 6) != 1 || memcmp(arp + 8, vnet_lab_psp_mac, 6)
            || memcmp(arp + 14, vnet_lab_psp_ip, 4)
            || memcmp(arp + 24, vnet_lab_gateway_ip, 4)) return 0;
        memcpy(reply, vnet_lab_psp_mac, 6);
        memcpy(reply + 6, vnet_lab_gateway_mac, 6);
        write16(reply + 12, 0x0806);
        memcpy(reply + 14, arp, 8);
        write16(reply + 20, 2);
        memcpy(reply + 22, vnet_lab_gateway_mac, 6);
        memcpy(reply + 28, vnet_lab_gateway_ip, 4);
        memcpy(reply + 32, vnet_lab_psp_mac, 6);
        memcpy(reply + 38, vnet_lab_psp_ip, 4);
        return 42;
    }
    if (type != 0x0800 || length < 42 || memcmp(frame, vnet_lab_gateway_mac, 6)) return 0;
    const uint8_t *ip = frame + 14, *udp = ip + 20;
    unsigned total = read16(ip + 2), udp_length = read16(udp + 4);
    if (ip[0] != 0x45 || ip[9] != 17 || !ip[8] || total < 28
        || total > length - 14 || capacity < total + 14u
        || (read16(ip + 6) & 0xbfffu) || vnet_checksum(ip, 20)
        || memcmp(ip + 12, vnet_lab_psp_ip, 4)
        || memcmp(ip + 16, vnet_lab_gateway_ip, 4)
        || udp_length != total - 20 || read16(udp + 2) != VNET_LAB_PORT
        || (read16(udp + 6) && udp_checksum(ip, udp, udp_length))) return 0;
    memcpy(reply, vnet_lab_psp_mac, 6);
    memcpy(reply + 6, vnet_lab_gateway_mac, 6);
    write16(reply + 12, 0x0800);
    uint8_t *out_ip = reply + 14, *out_udp = out_ip + 20;
    memset(out_ip, 0, 20);
    out_ip[0] = 0x45; out_ip[8] = 64; out_ip[9] = 17;
    write16(out_ip + 2, (uint16_t)total);
    memcpy(out_ip + 12, vnet_lab_gateway_ip, 4);
    memcpy(out_ip + 16, vnet_lab_psp_ip, 4);
    write16(out_ip + 10, vnet_checksum(out_ip, 20));
    memcpy(out_udp, udp, udp_length);
    write16(out_udp, VNET_LAB_PORT);
    write16(out_udp + 2, read16(udp));
    write16(out_udp + 6, 0);
    uint16_t checksum = udp_checksum(out_ip, out_udp, udp_length);
    write16(out_udp + 6, checksum ? checksum : 0xffffu);
    return total + 14u;
}
