#ifndef WIFI_TLS_CONTROL_H
#define WIFI_TLS_CONTROL_H
#define WIFI_TLS_MAGIC 0x57544c31u
#define WIFI_TLS_COUNT 11
#define WIFI_TLS_ABI 1
/* Internal owned-module hooks; values never contain TLS secrets. */
enum {WT_CONNECT,WT_BIO,WT_SETBIO,WT_HANDSHAKE,WT_READ,WT_WRITE,WT_ERROR,WT_READY,WT_SHUTDOWN,WT_FREE,WT_CLOSE};
typedef struct {
    unsigned magic,abi,callbacks[WIFI_TLS_COUNT],originals[WIFI_TLS_COUNT];
    volatile int status,owner;volatile unsigned active;
} wifi_tls_control_t;
#endif
