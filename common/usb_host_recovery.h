#ifndef PSP_USB_HOST_RECOVERY_H
#define PSP_USB_HOST_RECOVERY_H
#include <stdint.h>
#define USB_HOST_ENUM_TIMEOUT_MS 15000
#define USB_HOST_ENUM_RETRY_LIMIT 3u
typedef struct {
    int waiting;
    unsigned attempts;
    int64_t since,settle_until;
} usb_host_recovery_t;
/* A previous PSP identity may authorize probing the same dedicated port before
 * its next descriptor completes. A positively identified foreign device never does. */
static inline int usb_host_recovery_eligible(int known,int seen,int foreign)
{return !foreign && (known || seen);}
/* Only an eligible PSP session on a connected, enabled port is retried.
 * No idle bulk-IN timeout: an established session may legitimately be silent.
 * A successful mount or physical disconnect starts a fresh recovery budget. */
static inline int usb_host_recovery_poll(usb_host_recovery_t *s,int64_t now,
                                        int connected,int known_psp,int mounted,int enabled)
{
    if(now<s->settle_until)return 0;
    if(!connected || !known_psp || !enabled || mounted) {
        s->waiting=0;s->attempts=0;s->since=now;return 0;
    }
    if(!s->waiting){s->waiting=1;s->since=now;return 0;}
    if(s->attempts>=USB_HOST_ENUM_RETRY_LIMIT || now-s->since<USB_HOST_ENUM_TIMEOUT_MS)return 0;
    ++s->attempts;s->since=now;return 1;
}
/* Host reinitialization briefly reports its own port as disconnected. Preserve
 * the known-device identity and retry budget across that artificial interval. */
static inline void usb_host_recovery_restarted(usb_host_recovery_t *s,int64_t now)
{s->settle_until=now+2000;}
#endif
