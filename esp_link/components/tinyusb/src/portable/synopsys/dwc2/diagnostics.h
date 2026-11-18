/* Project-local read-only DWC2 diagnostics. No payloads or addresses exported. */
#ifndef PSP_DWC2_DIAGNOSTICS_H
#define PSP_DWC2_DIAGNOSTICS_H
#include <stdint.h>
#define PSP_DWC2_DIAG_CHANNELS 16u
typedef struct {
    uint32_t hcchar,hctsiz,hcint,hcintmsk,transferred,capacity;
    uint8_t allocated,endpoint;
} psp_dwc2_channel_diag_t;
unsigned hcd_dwc2_diag_channels(uint8_t rhport,psp_dwc2_channel_diag_t *out,unsigned capacity);
/* Called in the receive ISR, phase0 before copy / phase1 after copy. Implementations
 * must be bounded, nonblocking and must not log. Default observer is a no-op. */
void hcd_dwc2_diag_rx_cb(unsigned phase,unsigned channel,unsigned bytes,unsigned offset,
                        unsigned capacity,const void *buffer);
#endif
