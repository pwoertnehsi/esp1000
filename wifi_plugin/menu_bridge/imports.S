#include "pspimport.s"
IMPORT_START ESPWifiMenu, 0x40010000
IMPORT_FUNC ESPWifiMenu, 0xF5A6437B, wifiMenuRegister
IMPORT_FUNC ESPWifiMenu, 0x55319E68, wifiMenuRelease

IMPORT_FUNC ESPWifiMenu, 0xFB071512, wifiNativeRegister
IMPORT_FUNC ESPWifiMenu, 0xBB7CEFBC, wifiNativeRefresh

IMPORT_FUNC ESPWifiMenu, 0xD27434B9, wifiNativePowerAcquire

IMPORT_FUNC ESPWifiMenu, 0x0E83D512, wifiNativePowerRelease

IMPORT_FUNC ESPWifiMenu, 0x907EE6BB, wifiTlsRegister

IMPORT_FUNC ESPWifiMenu, 0x602897EB, wifiNativeOwnerAlive

IMPORT_FUNC ESPWifiMenu, 0x335469A2, wifiNativeCallbackValid
