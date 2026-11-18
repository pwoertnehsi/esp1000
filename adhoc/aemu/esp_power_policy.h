/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ESP_ADHOC_POWER_POLICY_H
#define ESP_ADHOC_POWER_POLICY_H
/* Syscon's USB power write has command 0x55, length 3, one payload byte.
 * Only rewrite Sony USB's power-off request while this game owns the rail. */
static inline int esp_power_hold_packet(unsigned char *packet, int owned)
{
    if (!owned || !packet || packet[12] != 0x55 || packet[13] != 3 || packet[14] != 0)
        return 0;
    packet[14] = 1;
    return 1;
}
#endif
