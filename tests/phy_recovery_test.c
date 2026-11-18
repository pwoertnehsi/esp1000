#include <assert.h>
#include <stdio.h>
#include "../esp_link/main/phy_recovery.c"
static int active,deleted,created,delete_error,create_error;
int tuh_inited(void){return active;}
esp_err_t usb_del_phy(usb_phy_handle_t p){assert(!active && p);++deleted;return delete_error;}
esp_err_t usb_new_phy(const usb_phy_config_t *c,usb_phy_handle_t *p)
{
    assert(!active && !*p && c->controller==USB_PHY_CTRL_OTG && c->target==USB_PHY_TARGET_INT);
    assert(c->otg_mode==USB_OTG_MODE_HOST && c->otg_speed==USB_PHY_SPEED_FULL);
    ++created;if(create_error)return create_error;*p=(void *)2;return 0;
}
int main(void)
{
    usb_phy_handle_t phy=(void *)1;
    assert(psp_usb_rebuild_phy(NULL)==ESP_ERR_INVALID_STATE);
    active=1;assert(psp_usb_rebuild_phy(&phy)==ESP_ERR_INVALID_STATE && !deleted && !created && phy==(void *)1);
    active=0;delete_error=-5;assert(psp_usb_rebuild_phy(&phy)==-5 && phy==(void *)1 && !created);
    delete_error=0;create_error=-7;assert(psp_usb_rebuild_phy(&phy)==-7 && !phy);
    create_error=0;assert(!psp_usb_rebuild_phy(&phy) && phy==(void *)2 && deleted==2 && created==2);
    assert(!psp_usb_rebuild_phy(&phy) && deleted==3 && created==3);
    puts("PASS: actual PHY recovery refuses live controller, retains failed-delete handle and retries failed allocation");
}
