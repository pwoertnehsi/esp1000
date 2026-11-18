#ifndef VNET_NATIVE_HTTP_H
#define VNET_NATIVE_HTTP_H
int vnet_run_native_http(int (*report)(const char *,int),const unsigned char remote_ip[4]);
int vnet_run_unbound_local_http(int (*report)(const char *,int));
int vnet_run_local_http(int (*report)(const char *,int));
int vnet_install_default_route(int (*report)(const char *,int));
#endif
