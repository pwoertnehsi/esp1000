#ifndef WIFI_NATIVE_ADAPTER_H
#define WIFI_NATIVE_ADAPTER_H
int wifi_native_adapter_start(void);
void wifi_native_log_drain(void);
void wifi_native_exit_log(const char *,unsigned);
void wifi_native_ssl_log(const char *,unsigned);
int wifi_native_adapter_stop(void);
#endif
