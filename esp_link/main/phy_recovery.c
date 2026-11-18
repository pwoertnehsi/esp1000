#include "phy_recovery.h"
#include "tusb.h"
esp_err_t psp_usb_rebuild_phy(usb_phy_handle_t *phy)
{
    if(!phy || tuh_inited())return ESP_ERR_INVALID_STATE;
    if(*phy) {
        esp_err_t r=usb_del_phy(*phy);
        if(r!=ESP_OK)return r;
        *phy=NULL;
    }
    const usb_phy_config_t config={.controller=USB_PHY_CTRL_OTG,.target=USB_PHY_TARGET_INT,
        .otg_mode=USB_OTG_MODE_HOST,.otg_speed=USB_PHY_SPEED_FULL};
    return usb_new_phy(&config,phy);
}
