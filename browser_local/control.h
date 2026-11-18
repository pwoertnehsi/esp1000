#ifndef PSP_BROWSER_CONTROL_H
#define PSP_BROWSER_CONTROL_H
#include <stdint.h>
#define BROWSER_START_MAGIC 0x42523401u
typedef struct {uint32_t magic,result_address,callback_module,callbacks[6];} browser_start_t;
typedef struct {
    int32_t result;
    uint32_t active,patches,observed,refused,restore_fail,last_uid,module_mask,apctl_hooks,browser_gates;
    int32_t last_result;
    uint32_t asset_events,asset_command;
    int32_t asset_result;
} browser_status_t;
#endif
