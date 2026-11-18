#include "pspimport.s"
IMPORT_START ESPWifiMenu, 0x40010000
IMPORT_FUNC ESPWifiMenu, 0xF5A6437B, wifiMenuRegister
IMPORT_FUNC ESPWifiMenu, 0x55319E68, wifiMenuRelease

IMPORT_FUNC ESPWifiMenu, 0xFB071512, wifiNativeRegister
IMPORT_FUNC ESPWifiMenu, 0xBB7CEFBC, wifiNativeRefresh
