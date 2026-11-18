/* Reuse the tested GAME power lease with a separate VSH context/owner. */
#define ESP_POWER_CONTEXT PSP_INIT_KEYCONFIG_VSH
#define ESP_POWER_MODULE SceModule
#define ESP_POWER_TAG "WIFI_POWER"
#define ESP_POWER_LOG "ms0:/wifi-power.log"
#define espAdhocPowerAcquire wifiNativePowerAcquire
#define espAdhocPowerRelease wifiNativePowerRelease
#include "../adhoc/aemu/esp_power.c"
int wifi_power_can_stop(void)
{
    return !owned && !changing && !calls && !pending;
}
