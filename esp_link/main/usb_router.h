#ifndef PSP_USB_ROUTER_H
#define PSP_USB_ROUTER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void usb_router_start(void);
void usb_router_link(bool enabled);
bool usb_router_receive(const void *data, unsigned length);
unsigned usb_router_take(void *data, unsigned capacity);
void usb_router_status(void);
#endif
