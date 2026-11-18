#ifndef BROWSER_ADAPTER_H
#define BROWSER_ADAPTER_H
#include <pspnet_apctl.h>
#include "../../virtual_net/control.h"
int adapter_start(volatile vnet_start_result_t *transport);
int adapter_pause(void);
void adapter_dispose(void);
int adapter_get_state(int *state);
int adapter_get_info(int code,union SceNetApctlInfo *info);
int adapter_connect(int profile);
int adapter_disconnect(void);
int adapter_add_handler(sceNetApctlHandler handler,void *arg);
int adapter_del_handler(int id);
#endif
