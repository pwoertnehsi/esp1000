#ifndef VNET_USB_TRANSPORT_H
#define VNET_USB_TRANSPORT_H
#include "native_wifi_scan.h"
#include "native_wifi_profile.h"
typedef struct {
    unsigned tx_busy,rx_busy,packet_total,packet_offset;
    unsigned tx_frames,rx_frames,tx_age_us,rx_age_us;
} vnet_usb_progress_t;
/* Called by the USB owner only, alongside poll; ages wrap at uint32 duration. */
void vnet_usb_progress(vnet_usb_progress_t *progress);
int vnet_usb_native_mode(int enabled);
int vnet_usb_profile_begin(unsigned request,const native_wifi_profile_t *profile);
int vnet_usb_profile_status(void);
int vnet_usb_scan_begin(unsigned request,unsigned limit);
int vnet_usb_scan_snapshot(native_scan_session_t *out);
int vnet_usb_start(void);
unsigned vnet_usb_start_step(void);
int vnet_usb_stop(void);
int vnet_usb_ready(void);
unsigned vnet_usb_uplink(unsigned char remote_ip[4]);
unsigned vnet_usb_errors(void);
int vnet_usb_wifi_info(unsigned char info[48]);
void vnet_usb_last_error(unsigned *stage, int *code, unsigned *bytes);
int vnet_usb_can_send(void);
int vnet_usb_send(const void *, unsigned);
int vnet_usb_poll(void *, unsigned);
#endif
