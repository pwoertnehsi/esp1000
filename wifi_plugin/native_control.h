#ifndef ESP_WIFI_NATIVE_CONTROL_H
#define ESP_WIFI_NATIVE_CONTROL_H
#include "../common/native_wifi_scan.h"
#include "../common/native_wifi_profile.h"
#define WIFI_NATIVE_USB_MAGIC 0x574e5501u
#define WIFI_NATIVE_CALLBACKS 15u
#define WIFI_NATIVE_HOOK_ABI 5u
#define WIFI_NATIVE_RADIO 4u
#define WIFI_NATIVE_CONSUMERS 5u
#define WIFI_NATIVE_USB_ABI 4u
#define WIFI_NATIVE_SCAN_COMMAND 1u
#define WIFI_NATIVE_CONNECT_COMMAND 2u
/* Single producer (user adapter worker), single consumer (USB owner). Sequence
 * fields publish after payload writes. Keys are added in the profile protocol,
 * not to diagnostics or scan results. */
typedef struct {
    unsigned magic,abi,request,completed;
    unsigned command;
    native_wifi_profile_t profile;
    int result;
    unsigned count,revision;
    native_scan_row_t rows[NATIVE_SCAN_LIMIT];
    volatile unsigned phase;
    volatile int detail;
} wifi_native_usb_control_t;
typedef struct {unsigned magic,address,owner;} wifi_native_usb_args_t;
#define WIFI_NATIVE_HOOK_MAGIC 0x574e4801u
enum {WIFI_NATIVE_GATE,WIFI_NATIVE_SCAN,WIFI_NATIVE_LIST,WIFI_NATIVE_ENTRY,
      WIFI_NATIVE_ADD,WIFI_NATIVE_DEL,WIFI_NATIVE_STATE,WIFI_NATIVE_LOGPTR,WIFI_NATIVE_LOGSET,
      WIFI_NATIVE_CONNECT,WIFI_NATIVE_DISCONNECT,WIFI_NATIVE_INFO,WIFI_NATIVE_RADIO_STATE,
      WIFI_NATIVE_RADIO_REQUEST,WIFI_NATIVE_RADIO_SEND};
typedef struct {
    unsigned magic,abi,callbacks[WIFI_NATIVE_CALLBACKS];
    unsigned radio_send_original;
    volatile int status,owner;
    volatile unsigned detail;
    volatile int browser_owners[WIFI_NATIVE_CONSUMERS];
    volatile int browser_status;
    volatile unsigned consumer_mask;
} wifi_native_hooks_t;
#define WIFI_NATIVE_SESSION_MAGIC 0x574e5301u
#define WIFI_NATIVE_SESSION_ABI 3u
typedef struct {
    unsigned magic,abi;
    volatile int cancel,ready,result;
    volatile int alive,recovering,finished;
    volatile unsigned recoveries,info_users;
    unsigned info_callback;
    volatile unsigned stage;
    volatile int detail;
} wifi_native_session_control_t;
#endif
