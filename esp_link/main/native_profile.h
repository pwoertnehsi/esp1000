#ifndef ESP_NATIVE_PROFILE_H
#define ESP_NATIVE_PROFILE_H
#include "native_wifi_profile.h"
void native_profile_start(void);
int native_profile_submit(const native_wifi_profile_t *,unsigned request,unsigned epoch);
int native_profile_take(link_frame_t *,unsigned *epoch);
int native_profile_busy(void);
#endif
