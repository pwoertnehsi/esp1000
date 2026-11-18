#ifndef PSP_PHY_RECOVERY_H
#define PSP_PHY_RECOVERY_H
#include "esp_private/usb_phy.h"
/* Controller must be stopped before replacing its PHY. Wi-Fi is untouched. */
esp_err_t psp_usb_rebuild_phy(usb_phy_handle_t *phy);
#endif
