#ifndef PSP_HOST_WAKEUP_H
#define PSP_HOST_WAKEUP_H
#include <stdbool.h>
/* Register the sole USB owner before enabling the host IRQ. It stays alive
 * for the application lifetime. Callbacks only wake it, never run USB APIs. */
void host_wakeup_bind(void);
void host_wakeup_signal(bool in_isr);
void host_wakeup_wait(void);
#endif
