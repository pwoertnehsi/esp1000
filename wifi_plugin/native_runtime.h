#ifndef WIFI_NATIVE_RUNTIME_H
#define WIFI_NATIVE_RUNTIME_H
int wifi_native_init(void);
int wifi_native_stop(void);
int wifi_native_status(void);
int wifi_native_browser_status(void);
unsigned wifi_native_detail(void);
int wifiNativeRefresh(void);
void wifi_native_observed(unsigned out[4]);
int wifiNativeRegister(unsigned address,unsigned size);
#endif
