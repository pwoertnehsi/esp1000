#ifndef ESP_NATIVE_SCAN_H
#define ESP_NATIVE_SCAN_H
#include "link_protocol.h"
void native_scan_start(void);
void native_scan_status(void);
int native_scan_busy(void);
/* USB owner thread supplies its connection epoch; old replies are discarded. */
int native_scan_request(const link_frame_t *request,unsigned epoch);
int native_scan_take(link_frame_t *result,unsigned *epoch);
#endif
